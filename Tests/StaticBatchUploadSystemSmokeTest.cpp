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

	StaticBatchShadowSubmissionTelemetry firstTile;
	firstTile.candidateGroupCount = 4;
	firstTile.visibleInstanceCount = 5;
	firstTile.culledInstanceCount = 2;
	firstTile.fullyCulledGroupCount = 1;
	firstTile.mixedVisibilityFallbackCount = 1;
	firstTile.submittedGroupCount = 1;
	firstTile.submittedInstanceCount = 3;
	firstTile.mappingFallbackCount = 1;
	firstTile.eligibilityFallbackCount = 1;
	firstTile.textureBindingFallbackCount = 1;
	firstTile.geometryFallbackCount = 1;
	system.RecordShadowSubmissionTelemetry(firstTile);

	StaticBatchShadowSubmissionTelemetry secondTile;
	secondTile.candidateGroupCount = 4;
	secondTile.visibleInstanceCount = 6;
	secondTile.culledInstanceCount = 1;
	secondTile.visibilityFailureCount = 1;
	secondTile.submittedGroupCount = 2;
	secondTile.submittedInstanceCount = 5;
	secondTile.layerFallbackCount = 1;
	secondTile.drawFailureCount = 1;
	secondTile.queueFailureCount = 1;
	system.RecordShadowSubmissionTelemetry(secondTile);

	const StaticBatchShadowSubmissionTelemetry& aggregate =
		system.GetShadowSubmissionTelemetry();
	assert(aggregate.lightTileCount == 2);
	assert(aggregate.submissionAttemptCount == 2);
	assert(aggregate.candidateGroupCount == 8);
	assert(aggregate.visibleInstanceCount == 11);
	assert(aggregate.culledInstanceCount == 3);
	assert(aggregate.fullyCulledGroupCount == 1);
	assert(aggregate.mixedVisibilityFallbackCount == 1);
	assert(aggregate.visibilityFailureCount == 1);
	assert(aggregate.submittedGroupCount == 3);
	assert(aggregate.submittedInstanceCount == 8);
	assert(aggregate.mappingFallbackCount == 1);
	assert(aggregate.eligibilityFallbackCount == 1);
	assert(aggregate.textureBindingFallbackCount == 1);
	assert(aggregate.layerFallbackCount == 1);
	assert(aggregate.geometryFallbackCount == 1);
	assert(aggregate.drawFailureCount == 1);
	assert(aggregate.queueFailureCount == 1);
	assert(aggregate.ReplacedOrdinaryDrawCount() == 8);
	assert(aggregate.StaticDrawCallCount() == 3);
	assert(aggregate.EstimatedDrawCallReduction() == 5);

	system.ResetTelemetry();
	const StaticBatchShadowSubmissionTelemetry& resetAggregate =
		system.GetShadowSubmissionTelemetry();
	assert(resetAggregate.lightTileCount == 0);
	assert(resetAggregate.submissionAttemptCount == 0);
	assert(resetAggregate.candidateGroupCount == 0);
	assert(resetAggregate.visibleInstanceCount == 0);
	assert(resetAggregate.culledInstanceCount == 0);
	assert(resetAggregate.fullyCulledGroupCount == 0);
	assert(resetAggregate.mixedVisibilityFallbackCount == 0);
	assert(resetAggregate.visibilityFailureCount == 0);
	assert(resetAggregate.submittedGroupCount == 0);
	assert(resetAggregate.submittedInstanceCount == 0);
	assert(resetAggregate.textureBindingFallbackCount == 0);
	assert(resetAggregate.EstimatedDrawCallReduction() == 0);

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
