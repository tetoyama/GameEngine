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

	assert(tasks.size() == 2);
	const SystemTask& geometryTask = tasks[0];
	assert(geometryTask.owner == &system);
	assert(
		geometryTask.name ==
		"StaticBatchUploadSystem.Geometry.Synchronize"
	);
	assert(geometryTask.domain == SystemTaskDomain::Render);
	assert(geometryTask.order.phase == SystemPhase::Default);
	assert(geometryTask.order.priority == 0);
	assert(geometryTask.threadAffinity == ThreadAffinity::MainThread);
	assert(static_cast<bool>(geometryTask.execute));

	const SystemTask& instanceTask = tasks[1];
	assert(instanceTask.owner == &system);
	assert(instanceTask.name == "StaticBatchUploadSystem.Instance.Upload");
	assert(instanceTask.domain == SystemTaskDomain::Render);
	assert(instanceTask.order.phase == SystemPhase::Default);
	assert(instanceTask.order.priority == 1);
	assert(instanceTask.threadAffinity == ThreadAffinity::MainThread);
	assert(static_cast<bool>(instanceTask.execute));

	assert(system.GetGeometryBindingCache().BindingCount() == 0);
	assert(system.GetGeometryBindingCache().ResolvedGroupCount() == 0);
	assert(!system.IsShadowPipelineReady());

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
	constexpr std::array<std::byte, 4> shadowPixelByteCode{};
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

	StaticBatchShadowPipelineResources& shadowResources =
		system.GetShadowPipelineResources();
	assert(shadowResources.Create(
		*device,
		resources.VertexShader(),
		shadowPixelByteCode
	));
	assert(shadowResources.IsReady());
	const RHI::PipelineStateDesc* shadowPipelineDesc =
		device->GetPipelineStateDesc(shadowResources.PipelineState());
	assert(shadowPipelineDesc);
	assert(shadowPipelineDesc->vertexShader == resources.VertexShader());
	assert(shadowPipelineDesc->renderTargets.colorAttachmentCount == 0);
	assert(
		shadowPipelineDesc->renderTargets.depthStencilFormat ==
		RHI::Format::D32_Float
	);

	RHI::CommandListCreateDesc commandDesc;
	commandDesc.queueType = RHI::CommandQueueType::Graphics;
	auto commandList = device->CreateCommandList(commandDesc);
	assert(commandList);
	commandList->Begin();
	assert(resources.Bind(*commandList));
	assert(commandList->SetPipelineState(shadowResources.PipelineState()));
	commandList->End();

	assert(shadowResources.Release(*device));
	assert(!shadowResources.IsAllocated());
	assert(resources.IsReady());
	assert(resources.Release(*device));
	assert(!resources.IsAllocated());
	assert(!resources.IsReady());
	return 0;
}
