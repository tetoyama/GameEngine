#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "Service/Graphics/RHI/Null/NullRHIBackend.h"
#include "Service/Graphics/RHI/RHIRenderGraph.h"

namespace {

void TestBackendRegistryAndCapabilities(){
	const bool registered = RHI::RegisterNullRHIBackend();
	assert(registered ||
		RHI::BackendRegistry::Instance().IsRegistered(RHI::BackendType::Null));

	std::unique_ptr<RHI::IRHIBackend> backend =
		RHI::BackendRegistry::Instance().Create(RHI::BackendType::Null);
	assert(backend);
	assert(backend->GetType() == RHI::BackendType::Null);
	assert(backend->IsSupported());
	assert(!backend->EnumerateAdapters().empty());

	RHI::DeviceCreateDesc createDesc;
	createDesc.swapChain.width = 1280;
	createDesc.swapChain.height = 720;

	std::unique_ptr<RHI::IRHIDevice> device =
		backend->CreateDevice(createDesc);
	assert(device);
	assert(device->GetBackendType() == RHI::BackendType::Null);

	const RHI::DeviceCapabilities& capabilities =
		device->GetCapabilities();
	assert(capabilities.backend == RHI::BackendType::Null);
	assert(capabilities.supportsCompute);
	assert(capabilities.supportsMultipleCommandQueues);
	assert(capabilities.supportsTimelineSynchronization);

	assert(device->GetQueue(RHI::CommandQueueType::Graphics));
	assert(device->GetQueue(RHI::CommandQueueType::Compute));
	assert(device->GetQueue(RHI::CommandQueueType::Copy));
}

void TestGenerationalResourceHandles(){
	RHI::DeviceCreateDesc createDesc;
	RHI::NullRHIDevice device(createDesc);

	RHI::BufferDesc desc;
	desc.byteSize = 256;
	desc.bindFlags = RHI::BufferBindFlags::Vertex;

	const RHI::BufferHandle first = device.CreateBuffer(desc, {});
	assert(first);
	assert(device.GetBufferDesc(first));
	assert(device.DestroyBuffer(first));
	assert(device.GetBufferDesc(first) == nullptr);

	const RHI::BufferHandle second = device.CreateBuffer(desc, {});
	assert(second);
	assert(second.index == first.index);
	assert(second.generation != first.generation);
	assert(device.GetBufferDesc(first) == nullptr);
	assert(device.GetBufferDesc(second));
}

void TestQueueSubmissionAndFence(){
	RHI::DeviceCreateDesc createDesc;
	RHI::NullRHIDevice device(createDesc);

	std::unique_ptr<RHI::IRHICommandList> commandList =
		device.CreateCommandList({RHI::CommandQueueType::Graphics, false});
	assert(commandList);
	assert(commandList->GetQueueType() == RHI::CommandQueueType::Graphics);

	RHI::PipelineStateDesc pipelineDesc;
	const RHI::PipelineStateHandle pipeline =
		device.CreatePipelineState(pipelineDesc);
	assert(pipeline);

	commandList->Begin();
	assert(commandList->SetPipelineState(pipeline));
	commandList->Draw(3, 0);
	commandList->End();

	std::unique_ptr<RHI::IRHIFence> fence = device.CreateFence(0);
	assert(fence);
	assert(fence->GetCompletedValue() == 0);

	std::array<RHI::IRHICommandList*, 1> lists{commandList.get()};
	RHI::QueueSubmitDesc submit;
	submit.commandLists = std::span<RHI::IRHICommandList* const>(lists);
	submit.signalFence = fence->GetHandle();
	submit.signalValue = 7;

	RHI::IRHICommandQueue* queue =
		device.GetQueue(RHI::CommandQueueType::Graphics);
	assert(queue);
	assert(queue->Submit(submit));
	assert(fence->Wait(7, 1'000'000));
	assert(fence->GetCompletedValue() == 7);

	std::unique_ptr<RHI::IRHICommandList> computeList =
		device.CreateCommandList({RHI::CommandQueueType::Compute, false});
	computeList->Begin();
	computeList->Dispatch(1, 1, 1);
	computeList->End();

	lists[0] = computeList.get();
	submit.commandLists = std::span<RHI::IRHICommandList* const>(lists);
	assert(!queue->Submit(submit));
}

void TestRenderGraphOnNullBackend(){
	RHI::DeviceCreateDesc createDesc;
	RHI::NullRHIDevice device(createDesc);
	std::unique_ptr<RHI::IRHICommandList> commandList =
		device.CreateCommandList({RHI::CommandQueueType::Graphics, false});

	RHI::RenderGraph graph;
	const auto gbuffer = graph.AddResource("GBuffer");
	const auto lighting = graph.AddResource("Lighting");
	std::vector<int> execution;

	graph.AddPass(
		"GBuffer",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Write(gbuffer);
		},
		[&](RHI::IRHICommandList& commands){
			execution.push_back(1);
			commands.Draw(3, 0);
		}
	);
	graph.AddPass(
		"Lighting",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(gbuffer);
			builder.Write(lighting);
		},
		[&](RHI::IRHICommandList&){ execution.push_back(2); }
	);
	graph.AddPass(
		"Present",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(lighting);
		},
		[&](RHI::IRHICommandList&){ execution.push_back(3); }
	);

	assert(graph.Compile());
	assert(graph.ExecutionOrder().size() == 3);
	assert(graph.Execute(*commandList));
	assert((execution == std::vector<int>{1, 2, 3}));
}

void TestSwapChainContract(){
	RHI::DeviceCreateDesc createDesc;
	createDesc.swapChain.width = 640;
	createDesc.swapChain.height = 360;
	RHI::NullRHIDevice device(createDesc);

	RHI::IRHISwapChain* swapChain = device.GetSwapChain();
	assert(swapChain);
	assert(swapChain->GetDesc().width == 640);
	assert(swapChain->Resize(1920, 1080));
	assert(swapChain->GetDesc().width == 1920);
	assert(swapChain->Present(false));
}

} // namespace

int main(){
	TestBackendRegistryAndCapabilities();
	TestGenerationalResourceHandles();
	TestQueueSubmissionAndFence();
	TestRenderGraphOnNullBackend();
	TestSwapChainContract();
	return 0;
}
