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
	assert(registered || RHI::BackendRegistry::Instance().IsRegistered(RHI::BackendType::Null));
	std::unique_ptr<RHI::IRHIBackend> backend =
		RHI::BackendRegistry::Instance().Create(RHI::BackendType::Null);
	assert(backend && backend->GetType() == RHI::BackendType::Null);
	assert(backend->IsSupported());
	assert(!backend->EnumerateAdapters().empty());

	RHI::DeviceCreateDesc createDesc;
	createDesc.swapChain.width = 1280;
	createDesc.swapChain.height = 720;
	std::unique_ptr<RHI::IRHIDevice> device = backend->CreateDevice(createDesc);
	assert(device && device->GetBackendType() == RHI::BackendType::Null);
	const auto& capabilities = device->GetCapabilities();
	assert(capabilities.supportsCompute);
	assert(capabilities.supportsMultipleCommandQueues);
	assert(capabilities.supportsTimelineSynchronization);
	assert(device->GetQueue(RHI::CommandQueueType::Graphics));
	assert(device->GetQueue(RHI::CommandQueueType::Compute));
	assert(device->GetQueue(RHI::CommandQueueType::Copy));
}

void TestGenerationalResourceHandles(){
	RHI::NullRHIDevice device({});
	RHI::BufferDesc desc;
	desc.byteSize = 256;
	desc.bindFlags = RHI::BufferBindFlags::Vertex;
	const RHI::BufferHandle first = device.CreateBuffer(desc, {});
	assert(first && device.GetBufferDesc(first));
	assert(device.DestroyBuffer(first));
	assert(device.GetBufferDesc(first) == nullptr);
	const RHI::BufferHandle second = device.CreateBuffer(desc, {});
	assert(second);
	assert(second.index == first.index);
	assert(second.generation != first.generation);
	assert(device.GetBufferDesc(first) == nullptr);
	assert(device.GetBufferDesc(second));
}

void TestViewAndSamplerContracts(){
	RHI::NullRHIDevice device({});

	RHI::BufferDesc bufferDesc;
	bufferDesc.byteSize = 64;
	bufferDesc.stride = 16;
	bufferDesc.structured = true;
	bufferDesc.bindFlags = RHI::BufferBindFlags::ShaderResource |
		RHI::BufferBindFlags::UnorderedAccess;
	const RHI::BufferHandle buffer = device.CreateBuffer(bufferDesc, {});
	assert(buffer);

	RHI::BufferViewDesc bufferSrvDesc;
	bufferSrvDesc.buffer = buffer;
	bufferSrvDesc.type = RHI::BufferViewType::ShaderResource;
	const RHI::BufferViewHandle bufferSrv = device.CreateBufferView(bufferSrvDesc);
	assert(bufferSrv && device.GetBufferViewDesc(bufferSrv));

	RHI::BufferViewDesc bufferUavDesc = bufferSrvDesc;
	bufferUavDesc.type = RHI::BufferViewType::UnorderedAccess;
	const RHI::BufferViewHandle bufferUav = device.CreateBufferView(bufferUavDesc);
	assert(bufferUav);
	assert(!device.DestroyBuffer(buffer));

	RHI::TextureDesc textureDesc;
	textureDesc.width = 8;
	textureDesc.height = 8;
	textureDesc.mipLevels = 2;
	textureDesc.bindFlags = RHI::TextureBindFlags::ShaderResource |
		RHI::TextureBindFlags::RenderTarget;
	const RHI::TextureHandle texture = device.CreateTexture(textureDesc, {}, 0);
	assert(texture);

	RHI::TextureViewDesc textureSrvDesc;
	textureSrvDesc.texture = texture;
	textureSrvDesc.type = RHI::TextureViewType::ShaderResource;
	textureSrvDesc.mipLevelCount = 2;
	const RHI::TextureViewHandle textureSrv = device.CreateTextureView(textureSrvDesc);
	assert(textureSrv && device.GetTextureViewDesc(textureSrv));

	RHI::TextureViewDesc renderViewDesc;
	renderViewDesc.texture = texture;
	renderViewDesc.type = RHI::TextureViewType::RenderTarget;
	const RHI::TextureViewHandle renderView = device.CreateTextureView(renderViewDesc);
	assert(renderView);

	RHI::TextureViewDesc invalidView = renderViewDesc;
	invalidView.baseMipLevel = 2;
	assert(!device.CreateTextureView(invalidView));
	assert(!device.DestroyTexture(texture));

	RHI::SamplerDesc samplerDesc;
	samplerDesc.anisotropyEnable = true;
	samplerDesc.maxAnisotropy = 8;
	const RHI::SamplerHandle sampler = device.CreateSampler(samplerDesc);
	assert(sampler && device.GetSamplerDesc(sampler));

	std::unique_ptr<RHI::IRHICommandList> graphics =
		device.CreateCommandList({RHI::CommandQueueType::Graphics, false});
	graphics->Begin();
	assert(graphics->SetBufferView(RHI::ShaderStage::Pixel, 0, bufferSrv));
	assert(graphics->SetTextureView(RHI::ShaderStage::Pixel, 0, textureSrv));
	assert(graphics->SetSampler(RHI::ShaderStage::Pixel, 0, sampler));
	RHI::RenderPassDesc renderPass;
	renderPass.colorAttachments.push_back({renderView});
	assert(graphics->BeginRenderPass(renderPass));
	graphics->EndRenderPass();
	graphics->End();

	std::unique_ptr<RHI::IRHICommandList> compute =
		device.CreateCommandList({RHI::CommandQueueType::Compute, false});
	compute->Begin();
	assert(compute->SetBufferView(RHI::ShaderStage::Compute, 0, bufferUav));
	assert(compute->SetSampler(RHI::ShaderStage::Compute, 0, sampler));
	compute->Dispatch(1, 1, 1);
	compute->End();

	std::unique_ptr<RHI::IRHICommandList> copy =
		device.CreateCommandList({RHI::CommandQueueType::Copy, false});
	copy->Begin();
	assert(!copy->SetTextureView(RHI::ShaderStage::Compute, 0, textureSrv));
	assert(!copy->SetSampler(RHI::ShaderStage::Compute, 0, sampler));
	copy->End();

	assert(device.DestroyBufferView(bufferSrv));
	assert(device.DestroyBufferView(bufferUav));
	assert(device.DestroyBuffer(buffer));
	assert(device.DestroyTextureView(textureSrv));
	assert(device.DestroyTextureView(renderView));
	assert(device.DestroyTexture(texture));
	assert(device.DestroySampler(sampler));
}

void TestResourceBarrierContract(){
	RHI::NullRHIDevice device({});
	RHI::BufferDesc bufferDesc;
	bufferDesc.byteSize = 64;
	bufferDesc.initialState = RHI::ResourceState::Common;
	const RHI::BufferHandle buffer = device.CreateBuffer(bufferDesc, {});
	RHI::TextureDesc textureDesc;
	textureDesc.width = 4;
	textureDesc.height = 4;
	textureDesc.initialState = RHI::ResourceState::ShaderResource;
	const RHI::TextureHandle texture = device.CreateTexture(textureDesc, {}, 0);
	assert(buffer && texture);

	auto commandList = device.CreateCommandList({RHI::CommandQueueType::Graphics, false});
	commandList->Begin();
	RHI::ResourceBarrierDesc bufferBarrier;
	bufferBarrier.buffer = buffer;
	bufferBarrier.before = RHI::ResourceState::Common;
	bufferBarrier.after = RHI::ResourceState::CopyDestination;
	assert(commandList->ResourceBarrier(std::span<const RHI::ResourceBarrierDesc>(&bufferBarrier, 1)));
	RHI::ResourceBarrierDesc textureBarrier;
	textureBarrier.texture = texture;
	textureBarrier.before = RHI::ResourceState::ShaderResource;
	textureBarrier.after = RHI::ResourceState::RenderTarget;
	assert(commandList->ResourceBarrier(std::span<const RHI::ResourceBarrierDesc>(&textureBarrier, 1)));
	RHI::ResourceBarrierDesc invalid = bufferBarrier;
	invalid.texture = texture;
	assert(!commandList->ResourceBarrier(std::span<const RHI::ResourceBarrierDesc>(&invalid, 1)));
	invalid = bufferBarrier;
	invalid.subresource = 0;
	assert(!commandList->ResourceBarrier(std::span<const RHI::ResourceBarrierDesc>(&invalid, 1)));
	invalid = bufferBarrier;
	invalid.after = RHI::ResourceState::Undefined;
	assert(!commandList->ResourceBarrier(std::span<const RHI::ResourceBarrierDesc>(&invalid, 1)));
	RHI::RenderPassDesc renderPass;
	assert(commandList->BeginRenderPass(renderPass));
	assert(!commandList->ResourceBarrier(std::span<const RHI::ResourceBarrierDesc>(&bufferBarrier, 1)));
	commandList->EndRenderPass();
	commandList->End();
}

void TestQueueSubmissionAndFence(){
	RHI::NullRHIDevice device({});
	auto commandList = device.CreateCommandList({RHI::CommandQueueType::Graphics, false});
	RHI::PipelineStateDesc pipelineDesc;
	const RHI::PipelineStateHandle pipeline = device.CreatePipelineState(pipelineDesc);
	commandList->Begin();
	assert(commandList->SetPipelineState(pipeline));
	commandList->Draw(3, 0);
	commandList->End();

	auto fence = device.CreateFence(0);
	std::array<RHI::IRHICommandList*, 1> lists{commandList.get()};
	RHI::QueueSubmitDesc submit;
	submit.commandLists = std::span<RHI::IRHICommandList* const>(lists);
	submit.signalFence = fence->GetHandle();
	submit.signalValue = 7;
	auto* queue = device.GetQueue(RHI::CommandQueueType::Graphics);
	assert(queue->Submit(submit));
	assert(fence->Wait(7, 1'000'000));
	assert(fence->GetCompletedValue() == 7);

	auto computeList = device.CreateCommandList({RHI::CommandQueueType::Compute, false});
	computeList->Begin();
	computeList->Dispatch(1, 1, 1);
	computeList->End();
	lists[0] = computeList.get();
	submit.commandLists = std::span<RHI::IRHICommandList* const>(lists);
	assert(!queue->Submit(submit));
}

void TestRenderGraphOnNullBackend(){
	RHI::NullRHIDevice device({});
	auto commandList = device.CreateCommandList({RHI::CommandQueueType::Graphics, false});
	RHI::RenderGraph graph;
	const auto gbuffer = graph.AddResource("GBuffer");
	const auto lighting = graph.AddResource("Lighting");
	std::vector<int> execution;
	graph.AddPass("GBuffer", [&](RHI::RenderGraphPassBuilder& b){ b.Write(gbuffer); }, [&](RHI::IRHICommandList& c){ execution.push_back(1); c.Draw(3, 0); });
	graph.AddPass("Lighting", [&](RHI::RenderGraphPassBuilder& b){ b.Read(gbuffer); b.Write(lighting); }, [&](RHI::IRHICommandList&){ execution.push_back(2); });
	graph.AddPass("Present", [&](RHI::RenderGraphPassBuilder& b){ b.Read(lighting); }, [&](RHI::IRHICommandList&){ execution.push_back(3); });
	assert(graph.Compile());
	assert(graph.ExecutionOrder().size() == 3);
	assert(graph.Execute(*commandList));
	assert((execution == std::vector<int>{1, 2, 3}));
}

void TestSwapChainContract(){
	RHI::DeviceCreateDesc desc;
	desc.swapChain.width = 640;
	desc.swapChain.height = 360;
	RHI::NullRHIDevice device(desc);
	auto* swapChain = device.GetSwapChain();
	assert(swapChain && swapChain->GetDesc().width == 640);
	assert(swapChain->Resize(1920, 1080));
	assert(swapChain->GetDesc().width == 1920);
	assert(swapChain->Present(false));
}

} // namespace

int main(){
	TestBackendRegistryAndCapabilities();
	TestGenerationalResourceHandles();
	TestViewAndSamplerContracts();
	TestResourceBarrierContract();
	TestQueueSubmissionAndFence();
	TestRenderGraphOnNullBackend();
	TestSwapChainContract();
	return 0;
}
