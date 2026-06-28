#include <array>
#include <cassert>
#include <cstddef>
#include <vector>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchUploadSystem.h"
#include "Service/Graphics/RHI/Null/NullRHIBackend.h"

int main(){
	SceneManagerContext context{};
	StaticBatchUploadSystem system(&context);
	std::vector<SystemTask> tasks;
	SystemScheduleBuilder builder(&system, 7, tasks);

	system.RegisterTasks(builder);

	assert(tasks.size() == 1);
	const SystemTask& task = tasks.front();
	assert(task.owner == &system);
	assert(task.name == "StaticBatchUploadSystem.Instance.Upload");
	assert(task.domain == SystemTaskDomain::Render);
	assert(task.order.phase == SystemPhase::Default);
	assert(task.order.priority == 0);
	assert(task.threadAffinity == ThreadAffinity::MainThread);
	assert(static_cast<bool>(task.execute));

	RHI::BackendRegistry registry;
	assert(RHI::RegisterNullRHIBackend(registry));
	auto backend = registry.Create(RHI::BackendType::Null);
	assert(backend);

	RHI::DeviceCreateDesc deviceDesc;
	deviceDesc.swapChain.width = 64;
	deviceDesc.swapChain.height = 64;
	auto device = backend->CreateDevice(deviceDesc);
	assert(device);

	StaticBatchPipelineResources& resources = system.GetPipelineResources();
	constexpr std::array<std::byte, 4> vertexByteCode{};
	constexpr std::array<std::byte, 4> pixelByteCode{};
	assert(!resources.Create(*device, {}, {}));
	assert(resources.Create(*device, vertexByteCode, pixelByteCode));
	assert(resources.IsAllocated());
	assert(resources.IsReady());

	const RHI::PipelineStateDesc* pipelineDesc =
		device->GetPipelineStateDesc(resources.PipelineState());
	assert(pipelineDesc);
	assert(pipelineDesc->inputLayout.size() == 10);
	assert(
		pipelineDesc->renderTargets.colorAttachmentCount ==
		StaticBatchPipelineContract::GBufferColorAttachmentCount
	);
	assert(pipelineDesc->renderTargets.colorFormats[5] == RHI::Format::RGBA32_UInt);
	assert(pipelineDesc->renderTargets.depthStencilFormat == RHI::Format::D32_Float);

	RHI::CommandListCreateDesc commandDesc;
	commandDesc.queueType = RHI::CommandQueueType::Graphics;
	auto commandList = device->CreateCommandList(commandDesc);
	assert(commandList);
	commandList->Begin();
	assert(resources.Bind(*commandList));
	commandList->End();

	assert(resources.Release(*device));
	assert(!resources.IsAllocated());
	assert(!resources.IsReady());
	return 0;
}
