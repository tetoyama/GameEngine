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
#include "System/Render/StaticBatch/StaticBatchModelGeometryRuntimeStorage.h"
#include "System/Render/StaticBatch/StaticBatchPipelineBootstrap.h"
#include "System/Render/StaticBatch/StaticBatchPipelineResources.h"
#include "System/Render/StaticBatch/StaticBatchShadowPipelineBootstrap.h"
#include "System/Render/StaticBatch/StaticBatchShadowPipelineResources.h"
#include "System/Render/StaticBatch/StaticBatchShadowSubmissionTelemetry.h"
#include "System/Render/StaticBatch/StaticBatchVisibleInstanceBuffer.h"

struct StaticBatchRhiOwnershipTelemetry {
	RHI::DeviceGeneration boundDeviceGeneration =
		RHI::InvalidDeviceGeneration;
	std::size_t deviceTransitionCount = 0;
	std::size_t resetCount = 0;
	std::size_t resetFailureCount = 0;
	std::size_t abandonCount = 0;
	bool hasAllocatedGpuResources = false;
};

class StaticBatchUploadSystem final : public ISystem {
public:
	explicit StaticBatchUploadSystem(SceneManagerContext* context)
		: m_context(context) {
	}

	const char* GetSystemName() const override {
		return "StaticBatchUploadSystem";
	}

	void Initialize() override {
		(void)ResolveBoundDevice();
	}

	void Finalize() override {
		(void)ResetGpuResources();
		m_modelGeometrySourceProvider.Reset();
		m_shadowVisibleInstances.Reset();
		m_lastUploadSucceeded = false;
		m_shadowSubmissionTelemetry.Reset();
	}

	void Stop() override {
		if(m_geometryBindingCache.BindingCount() != 0){
			if(RHI::IRHIDevice* device = ResolveReleaseDevice()){
				if(!m_geometryBindingCache.Release(*device)){
					++m_rhiResetFailureCount;
				}
			}else{
				m_geometryBindingCache.Abandon();
				++m_rhiAbandonCount;
			}
		}
		m_modelGeometrySourceProvider.Reset();
		m_lastUploadSucceeded = false;
	}

	void RegisterTasks(SystemScheduleBuilder& builder) override {
		SystemAccess geometryAccess;
		geometryAccess
			.ReadComponent<ModelRendererComponent>()
			.ReadResource<ModelData>()
			.ReadResource<RenderPacketFrameBuffer>()
			.WriteResource<StaticBatchModelGeometryRuntimeStorage>()
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

	const StaticBatchModelGeometryRuntimeStorage&
	GetModelGeometryRuntimeStorage() const noexcept {
		return m_modelGeometrySourceProvider.Storage();
	}

	StaticBatchVisibleInstanceBuffer&
	GetShadowVisibleInstanceBuffer() const noexcept {
		return m_shadowVisibleInstances;
	}

	StaticBatchGpuInstanceBuffer&
	GetShadowVisibleGpuInstanceBuffer() const noexcept {
		return m_shadowVisibleGpuInstanceBuffer;
	}

	StaticBatchGpuInstanceBufferTelemetry GetTelemetry() const noexcept {
		return m_gpuInstanceBuffer.Telemetry();
	}

	StaticBatchGeometryBindingCacheTelemetry GetGeometryTelemetry() const noexcept {
		return m_geometryBindingCache.Telemetry();
	}

	StaticBatchModelGeometryRuntimeStorageTelemetry
	GetModelGeometryRuntimeTelemetry() const noexcept {
		return m_modelGeometrySourceProvider.Storage().Telemetry();
	}

	StaticBatchRhiOwnershipTelemetry
	GetRhiOwnershipTelemetry() const noexcept {
		return {
			m_boundDeviceGeneration,
			m_rhiDeviceTransitionCount,
			m_rhiResetCount,
			m_rhiResetFailureCount,
			m_rhiAbandonCount,
			HasAllocatedGpuResources()
		};
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
		m_modelGeometrySourceProvider.Storage().ResetMetrics();
		m_shadowSubmissionTelemetry.Reset();
		m_rhiDeviceTransitionCount = 0;
		m_rhiResetCount = 0;
		m_rhiResetFailureCount = 0;
		m_rhiAbandonCount = 0;
	}

	bool LastUploadSucceeded() const noexcept {
		return m_lastUploadSucceeded;
	}

	RHI::DeviceGeneration BoundDeviceGeneration() const noexcept {
		return m_boundDeviceGeneration;
	}

	bool ResetGpuResources() noexcept {
		++m_rhiResetCount;
		if(!HasAllocatedGpuResources()){
			ClearDeviceBinding();
			ResetGpuRuntimeState();
			return true;
		}

		if(RHI::IRHIDevice* device = ResolveReleaseDevice()){
			if(ReleaseAllGpuResources(*device)){
				ClearDeviceBinding();
				ResetGpuRuntimeState();
				return true;
			}
			++m_rhiResetFailureCount;
			return false;
		}

		AbandonAllGpuResources();
		ClearDeviceBinding();
		++m_rhiResetFailureCount;
		return false;
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

	RHI::RenderHardwareInterfaceService* ResolveRHIService() const noexcept {
		if(!m_context || !m_context->graphics) return nullptr;
		return m_context->graphics->GetRHIService();
	}

	bool IsBoundTo(
		const RHI::IRHIDevice& device,
		RHI::DeviceGeneration generation
	) const noexcept {
		return m_boundDevice == &device &&
			m_boundDeviceGeneration == generation &&
			generation != RHI::InvalidDeviceGeneration &&
			!m_boundDeviceLifetime.expired();
	}

	RHI::IRHIDevice* ResolveReleaseDevice() const noexcept {
		RHI::RenderHardwareInterfaceService* service = ResolveRHIService();
		const RHI::DeviceOwnershipSnapshot ownership = service
			? service->GetDeviceOwnershipSnapshot()
			: RHI::DeviceOwnershipSnapshot{};
		return ownership.IsValid() &&
			IsBoundTo(*ownership.device, ownership.generation)
			? ownership.device
			: nullptr;
	}

	RHI::IRHIDevice* ResolveBoundDevice(){
		RHI::RenderHardwareInterfaceService* service = ResolveRHIService();
		const RHI::DeviceOwnershipSnapshot ownership = service
			? service->GetDeviceOwnershipSnapshot()
			: RHI::DeviceOwnershipSnapshot{};
		RHI::IRHIDevice* device = ownership.device;
		const RHI::DeviceGeneration generation = ownership.generation;

		if(!ownership.IsValid()){
			if(m_boundDevice || HasAllocatedGpuResources()){
				++m_rhiDeviceTransitionCount;
				AbandonAllGpuResources();
				ClearDeviceBinding();
			}
			return nullptr;
		}

		if(IsBoundTo(*device, generation)){
			BootstrapPipelines(*device);
			return device;
		}

		// Device identity、Lifetime、Generationのいずれかが変わった場合、
		// 旧Handleを新DeviceへDestroyしない。旧Resource Poolの破棄に委ねる。
		if(m_boundDevice || HasAllocatedGpuResources()){
			++m_rhiDeviceTransitionCount;
			AbandonAllGpuResources();
		}
		m_boundDevice = device;
		m_boundDeviceLifetime = ownership.lifetime;
		m_boundDeviceGeneration = generation;
		BootstrapPipelines(*device);
		return device;
	}

	void BootstrapPipelines(RHI::IRHIDevice& device){
		if(m_pipelineBootstrapResult ==
			StaticBatchPipelineBootstrapResult::NotAttempted){
			m_pipelineBootstrapResult = StaticBatchPipelineBootstrap::Initialize(
				&device,
				m_pipelineResources
			);
		}
		if(m_shadowPipelineBootstrapResult ==
			StaticBatchShadowPipelineBootstrapResult::NotAttempted){
			m_shadowPipelineBootstrapResult =
				StaticBatchShadowPipelineBootstrap::Initialize(
					&device,
					m_pipelineResources,
					m_shadowPipelineResources
				);
		}
	}

	bool HasAllocatedGpuResources() const noexcept {
		return m_pipelineResources.IsAllocated() ||
			m_shadowPipelineResources.IsAllocated() ||
			m_geometryBindingCache.BindingCount() != 0 ||
			static_cast<bool>(m_gpuInstanceBuffer.Buffer()) ||
			static_cast<bool>(m_shadowVisibleGpuInstanceBuffer.Buffer());
	}

	bool ReleaseAllGpuResources(RHI::IRHIDevice& device) noexcept {
		bool releasedAll = true;
		if(!m_shadowVisibleGpuInstanceBuffer.Release(device)){
			releasedAll = false;
		}

		const bool shadowReleased =
			m_shadowPipelineResources.Release(device);
		if(!shadowReleased){
			releasedAll = false;
		}

		if(!m_geometryBindingCache.Release(device)){
			releasedAll = false;
		}

		if(shadowReleased){
			if(!m_pipelineResources.Release(device)){
				releasedAll = false;
			}
		}else if(m_pipelineResources.IsAllocated()){
			releasedAll = false;
		}

		if(!m_gpuInstanceBuffer.Release(device)){
			releasedAll = false;
		}
		return releasedAll && !HasAllocatedGpuResources();
	}

	void AbandonAllGpuResources() noexcept {
		const bool hadAllocatedResources = HasAllocatedGpuResources();
		m_shadowVisibleGpuInstanceBuffer.Abandon();
		m_shadowPipelineResources.Abandon();
		m_geometryBindingCache.Abandon();
		m_pipelineResources.Abandon();
		m_gpuInstanceBuffer.Abandon();
		ResetGpuRuntimeState();
		if(hadAllocatedResources){
			++m_rhiAbandonCount;
		}
	}

	void ResetGpuRuntimeState() noexcept {
		m_modelGeometrySourceProvider.Reset();
		m_shadowVisibleInstances.Reset();
		m_shadowPipelineBootstrapResult =
			StaticBatchShadowPipelineBootstrapResult::NotAttempted;
		m_pipelineBootstrapResult =
			StaticBatchPipelineBootstrapResult::NotAttempted;
		m_lastUploadSucceeded = false;
	}

	void ClearDeviceBinding() noexcept {
		m_boundDevice = nullptr;
		m_boundDeviceLifetime.reset();
		m_boundDeviceGeneration = RHI::InvalidDeviceGeneration;
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
		m_modelGeometrySourceProvider.BeginSynchronization();

		RenderSystem* renderSystem = ResolveRenderSystem();
		RHI::IRHIDevice* device = ResolveBoundDevice();
		if(!renderSystem || !device){
			m_modelGeometrySourceProvider.EndSynchronization();
			return;
		}

		const RenderPacketFrameBuffer& frameBuffer =
			renderSystem->GetRenderPacketBuffer();
		const StaticBatchInstanceDataBuffer& source =
			frameBuffer.StaticBatchInstances();
		if(!source.IsValid() || source.IsOverflowed()){
			m_geometryBindingCache.Synchronize(
				*device,
				std::span<const StaticBatchPacketCacheEntry>{},
				frameBuffer.Packets(),
				m_modelGeometrySourceProvider
			);
			m_modelGeometrySourceProvider.EndSynchronization();
			return;
		}

		m_geometryBindingCache.Synchronize(
			*device,
			source.Groups(),
			frameBuffer.Packets(),
			m_modelGeometrySourceProvider
		);
		m_modelGeometrySourceProvider.EndSynchronization();
	}

	void UploadInstances(){
		m_lastUploadSucceeded = false;

		RenderSystem* renderSystem = ResolveRenderSystem();
		RHI::IRHIDevice* device = ResolveBoundDevice();
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
	RHI::IRHIDevice* m_boundDevice = nullptr;
	RHI::IRHIDevice::LifetimeToken m_boundDeviceLifetime;
	RHI::DeviceGeneration m_boundDeviceGeneration =
		RHI::InvalidDeviceGeneration;
	StaticBatchGpuInstanceBuffer m_gpuInstanceBuffer;
	StaticBatchPipelineResources m_pipelineResources;
	StaticBatchShadowPipelineResources m_shadowPipelineResources;
	StaticBatchRuntimeModelGeometrySourceProvider m_modelGeometrySourceProvider;
	StaticBatchGeometryBindingCache m_geometryBindingCache;
	StaticBatchPipelineBootstrapResult m_pipelineBootstrapResult =
		StaticBatchPipelineBootstrapResult::NotAttempted;
	StaticBatchShadowPipelineBootstrapResult m_shadowPipelineBootstrapResult =
		StaticBatchShadowPipelineBootstrapResult::NotAttempted;
	mutable StaticBatchVisibleInstanceBuffer m_shadowVisibleInstances;
	mutable StaticBatchGpuInstanceBuffer m_shadowVisibleGpuInstanceBuffer;
	mutable StaticBatchShadowSubmissionTelemetry m_shadowSubmissionTelemetry;
	std::size_t m_rhiDeviceTransitionCount = 0;
	std::size_t m_rhiResetCount = 0;
	std::size_t m_rhiResetFailureCount = 0;
	std::size_t m_rhiAbandonCount = 0;
	bool m_lastUploadSucceeded = false;
};
