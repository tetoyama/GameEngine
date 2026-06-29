#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>

#include "Interface/ISystem.h"
#include "Graphics/graphicsContext.h"
#include "Graphics/RHI/RHIService.h"
#include "Registry/systemRegistry.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchGpuInstanceBuffer.h"
#include "System/Render/StaticBatch/StaticBatchGeometryBindingCache.h"
#include "System/Render/StaticBatch/StaticBatchPipelineBootstrap.h"
#include "System/Render/StaticBatch/StaticBatchPipelineResources.h"
#include "System/Render/StaticBatch/StaticBatchShadowPipelineBootstrap.h"
#include "System/Render/StaticBatch/StaticBatchShadowPipelineResources.h"
#include "System/Render/StaticBatch/StaticBatchShadowSubmissionTelemetry.h"

class StaticBatchUploadSystem final : public ISystem {
public:
	explicit StaticBatchUploadSystem(SceneManagerContext* context)
		: m_context(context) {
	}

	const char* GetSystemName() const override {
		return "StaticBatchUploadSystem";
	}

	void Initialize() override {
		RHI::IRHIDevice* device = ResolveDevice();
		m_pipelineBootstrapResult = StaticBatchPipelineBootstrap::Initialize(
			device,
			m_pipelineResources
		);
		m_shadowPipelineBootstrapResult =
			StaticBatchShadowPipelineBootstrap::Initialize(
				device,
				m_pipelineResources,
				m_shadowPipelineResources
			);
	}

	void Finalize() override {
		RHI::IRHIDevice* device = ResolveDevice();
		if(device){
			const bool shadowReleased =
				m_shadowPipelineResources.Release(*device);
			m_geometryBindingCache.Release(*device);
			if(shadowReleased){
				m_pipelineResources.Release(*device);
			}
			m_gpuInstanceBuffer.Release(*device);
		}
		m_shadowPipelineBootstrapResult =
			StaticBatchShadowPipelineBootstrapResult::NotAttempted;
		m_pipelineBootstrapResult =
			StaticBatchPipelineBootstrapResult::NotAttempted;
		m_lastUploadSucceeded = false;
		m_shadowSubmissionTelemetry.Reset();
	}

	void Stop() override {
		m_lastUploadSucceeded = false;
	}

	void RegisterTasks(SystemScheduleBuilder& builder) override {
		SystemAccess geometryAccess;
		geometryAccess
			.ReadResource<RenderPacketFrameBuffer>()
			.WriteResource<StaticBatchGeometryBindingCache>();

		builder.AddTask(
			"StaticBatchUploadSystem.Geometry.Synchronize",
			SystemTaskDomain::Render,
			SystemPhase::Default,
			0,
			std::move(geometryAccess),
			ThreadAffinity::MainThread,
			[this](const SystemTaskContext&){
				SynchronizeGeometry();
			}
		);

		SystemAccess instanceAccess;
		instanceAccess
			.ReadResource<RenderPacketFrameBuffer>()
			.WriteResource<StaticBatchGpuInstanceBuffer>();

		builder.AddTask(
			"StaticBatchUploadSystem.Instance.Upload",
			SystemTaskDomain::Render,
			SystemPhase::Default,
			1,
			std::move(instanceAccess),
			ThreadAffinity::MainThread,
			[this](const SystemTaskContext&){
				UploadInstances();
			}
		);
	}

	const StaticBatchGpuInstanceBuffer& GetGpuInstanceBuffer() const noexcept {
		return m_gpuInstanceBuffer;
	}

	StaticBatchPipelineResources& GetPipelineResources() noexcept {
		return m_pipelineResources;
	}

	const StaticBatchPipelineResources& GetPipelineResources() const noexcept {
		return m_pipelineResources;
	}

	StaticBatchPipelineBootstrapResult GetPipelineBootstrapResult() const noexcept {
		return m_pipelineBootstrapResult;
	}

	bool IsPipelineReady() const noexcept {
		return m_pipelineBootstrapResult ==
			StaticBatchPipelineBootstrapResult::Success &&
			m_pipelineResources.IsReady();
	}

	StaticBatchShadowPipelineResources& GetShadowPipelineResources() noexcept {
		return m_shadowPipelineResources;
	}

	const StaticBatchShadowPipelineResources&
	GetShadowPipelineResources() const noexcept {
		return m_shadowPipelineResources;
	}

	StaticBatchShadowPipelineBootstrapResult
	GetShadowPipelineBootstrapResult() const noexcept {
		return m_shadowPipelineBootstrapResult;
	}

	bool IsShadowPipelineReady() const noexcept {
		return m_shadowPipelineBootstrapResult ==
			StaticBatchShadowPipelineBootstrapResult::Success &&
			m_shadowPipelineResources.IsReady();
	}

	StaticBatchGeometryBindingCache& GetGeometryBindingCache() noexcept {
		return m_geometryBindingCache;
	}

	const StaticBatchGeometryBindingCache& GetGeometryBindingCache() const noexcept {
		return m_geometryBindingCache;
	}

	StaticBatchGpuInstanceBufferTelemetry GetTelemetry() const noexcept {
		return m_gpuInstanceBuffer.Telemetry();
	}

	StaticBatchGeometryBindingCacheTelemetry GetGeometryTelemetry() const noexcept {
		return m_geometryBindingCache.Telemetry();
	}

	const StaticBatchShadowSubmissionTelemetry&
	GetShadowSubmissionTelemetry() const noexcept {
		return m_shadowSubmissionTelemetry;
	}

	void RecordShadowSubmissionTelemetry(
		const StaticBatchShadowSubmissionTelemetry& telemetry
	) const noexcept {
		++m_shadowSubmissionTelemetry.lightTileCount;
		++m_shadowSubmissionTelemetry.submissionAttemptCount;
		m_shadowSubmissionTelemetry.AccumulateSubmission(telemetry);
	}

	void ResetTelemetry() noexcept {
		m_gpuInstanceBuffer.ResetMetrics();
		m_geometryBindingCache.ResetMetrics();
		m_shadowSubmissionTelemetry.Reset();
	}

	bool LastUploadSucceeded() const noexcept {
		return m_lastUploadSucceeded;
	}

private:
	struct StaticBatchGpuStoragePolicy {
		std::size_t reserveCount = 0;
		bool allowRuntimeGrowth = true;
		bool valid = true;
	};

	RenderSystem* ResolveRenderSystem() const noexcept {
		if(!m_context || !m_context->sceneManager) return nullptr;
		SystemRegistry* registry = m_context->sceneManager->GetSystemRegistry();
		return registry ? registry->GetSystem<RenderSystem>() : nullptr;
	}

	RHI::IRHIDevice* ResolveDevice() const noexcept {
		if(!m_context || !m_context->graphics) return nullptr;
		RHI::RenderHardwareInterfaceService* service =
			m_context->graphics->GetRHIService();
		return service ? service->GetDevice() : nullptr;
	}

	StaticBatchGpuStoragePolicy ResolveStoragePolicy(
		const StaticBatchInstanceDataBuffer& source,
		const RenderPacketFrameBuffer& frameBuffer
	) const noexcept {
		StaticBatchGpuStoragePolicy policy;
		if(!m_context || !m_context->sceneManager){
			policy.valid = false;
			return policy;
		}

		for(const auto& sceneEntry : m_context->sceneManager->GetActiveScenes()){
			const std::shared_ptr<Scene>& scene = sceneEntry.second;
			SceneContext* sceneContext = scene ? scene->GetSceneContext() : nullptr;
			if(!sceneContext){
				policy.valid = false;
				return policy;
			}

			const std::size_t reserve = static_cast<std::size_t>(
				sceneContext->storageConfig.staticBatchReserve
			);
			if(reserve >
				(std::numeric_limits<std::size_t>::max)() - policy.reserveCount){
				policy.valid = false;
				return policy;
			}
			policy.reserveCount += reserve;
			policy.allowRuntimeGrowth =
				policy.allowRuntimeGrowth &&
				sceneContext->storageConfig.allowRuntimeGrowth;
		}

		const std::span<const RenderPacket> packets = frameBuffer.Packets();
		for(const StaticBatchInstanceGroup& group : source.Groups()){
			if(group.representativePacketIndex >= packets.size()){
				policy.valid = false;
				return policy;
			}

			const RenderPacket& packet = packets[group.representativePacketIndex];
			SceneContext* sceneContext = packet.bindings.sceneContext;
			if(!sceneContext ||
				packet.sceneContextID != group.sceneContextID ||
				sceneContext->contextID != group.sceneContextID){
				policy.valid = false;
				return policy;
			}
		}
		return policy;
	}

	void SynchronizeGeometry(){
		RenderSystem* renderSystem = ResolveRenderSystem();
		RHI::IRHIDevice* device = ResolveDevice();
		if(!renderSystem || !device) return;

		const RenderPacketFrameBuffer& frameBuffer =
			renderSystem->GetRenderPacketBuffer();
		const StaticBatchInstanceDataBuffer& source =
			frameBuffer.StaticBatchInstances();
		if(!source.IsValid() || source.IsOverflowed()){
			m_geometryBindingCache.Synchronize(
				*device,
				std::span<const StaticBatchPacketCacheEntry>{},
				frameBuffer.Packets()
			);
			return;
		}

		m_geometryBindingCache.Synchronize(
			*device,
			source.Groups(),
			frameBuffer.Packets()
		);
	}

	void UploadInstances(){
		m_lastUploadSucceeded = false;

		RenderSystem* renderSystem = ResolveRenderSystem();
		RHI::IRHIDevice* device = ResolveDevice();
		if(!renderSystem || !device) return;

		const RenderPacketFrameBuffer& frameBuffer =
			renderSystem->GetRenderPacketBuffer();
		const StaticBatchInstanceDataBuffer& source =
			frameBuffer.StaticBatchInstances();
		if(!source.IsValid() || source.IsOverflowed()) return;

		const StaticBatchGpuStoragePolicy storagePolicy =
			ResolveStoragePolicy(source, frameBuffer);
		if(!storagePolicy.valid ||
			!m_gpuInstanceBuffer.Reserve(*device, storagePolicy.reserveCount)){
			return;
		}

		RHI::CommandListCreateDesc commandDesc;
		commandDesc.queueType = RHI::CommandQueueType::Graphics;
		std::unique_ptr<RHI::IRHICommandList> commandList =
			device->CreateCommandList(commandDesc);
		if(!commandList) return;

		commandList->Begin();
		const bool uploadSucceeded = m_gpuInstanceBuffer.Synchronize(
			*device,
			*commandList,
			source,
			storagePolicy.allowRuntimeGrowth
		);
		commandList->End();
		if(!uploadSucceeded) return;

		RHI::IRHICommandQueue* queue =
			device->GetQueue(RHI::CommandQueueType::Graphics);
		if(!queue){
			m_gpuInstanceBuffer.Release(*device);
			return;
		}

		std::array<RHI::IRHICommandList*, 1> commandLists{
			commandList.get()
		};
		RHI::QueueSubmitDesc submitDesc;
		submitDesc.commandLists = commandLists;
		if(!queue->Submit(submitDesc)){
			m_gpuInstanceBuffer.Release(*device);
			return;
		}

		m_lastUploadSucceeded = true;
	}

	SceneManagerContext* m_context = nullptr;
	StaticBatchGpuInstanceBuffer m_gpuInstanceBuffer;
	StaticBatchPipelineResources m_pipelineResources;
	StaticBatchShadowPipelineResources m_shadowPipelineResources;
	StaticBatchGeometryBindingCache m_geometryBindingCache;
	StaticBatchPipelineBootstrapResult m_pipelineBootstrapResult =
		StaticBatchPipelineBootstrapResult::NotAttempted;
	StaticBatchShadowPipelineBootstrapResult m_shadowPipelineBootstrapResult =
		StaticBatchShadowPipelineBootstrapResult::NotAttempted;
	mutable StaticBatchShadowSubmissionTelemetry m_shadowSubmissionTelemetry;
	bool m_lastUploadSucceeded = false;
};
