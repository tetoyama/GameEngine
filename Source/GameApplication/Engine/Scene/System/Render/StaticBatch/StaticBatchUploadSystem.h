#pragma once

#include <array>
#include <memory>
#include <span>

#include "Interface/ISystem.h"
#include "Graphics/graphicsContext.h"
#include "Graphics/RHI/RHIService.h"
#include "Registry/systemRegistry.h"
#include "Scene/sceneManager.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchGpuInstanceBuffer.h"
#include "System/Render/StaticBatch/StaticBatchGeometryBindingCache.h"
#include "System/Render/StaticBatch/StaticBatchPipelineBootstrap.h"
#include "System/Render/StaticBatch/StaticBatchPipelineResources.h"

class StaticBatchUploadSystem final : public ISystem {
public:
	explicit StaticBatchUploadSystem(SceneManagerContext* context)
		: m_context(context) {
	}

	const char* GetSystemName() const override {
		return "StaticBatchUploadSystem";
	}

	void Initialize() override {
		m_pipelineBootstrapResult = StaticBatchPipelineBootstrap::Initialize(
			ResolveDevice(),
			m_pipelineResources
		);
	}

	void Finalize() override {
		RHI::IRHIDevice* device = ResolveDevice();
		if(device){
			m_geometryBindingCache.Release(*device);
			m_pipelineResources.Release(*device);
			m_gpuInstanceBuffer.Release(*device);
		}
		m_pipelineBootstrapResult =
			StaticBatchPipelineBootstrapResult::NotAttempted;
		m_lastUploadSucceeded = false;
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

	void ResetTelemetry() noexcept {
		m_gpuInstanceBuffer.ResetMetrics();
		m_geometryBindingCache.ResetMetrics();
	}

	bool LastUploadSucceeded() const noexcept {
		return m_lastUploadSucceeded;
	}

private:
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

		const StaticBatchInstanceDataBuffer& source =
			renderSystem->GetRenderPacketBuffer().StaticBatchInstances();
		if(!source.IsValid() || source.IsOverflowed()) return;

		RHI::CommandListCreateDesc commandDesc;
		commandDesc.queueType = RHI::CommandQueueType::Graphics;
		std::unique_ptr<RHI::IRHICommandList> commandList =
			device->CreateCommandList(commandDesc);
		if(!commandList) return;

		commandList->Begin();
		const bool uploadSucceeded = m_gpuInstanceBuffer.Synchronize(
			*device,
			*commandList,
			source
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
	StaticBatchGeometryBindingCache m_geometryBindingCache;
	StaticBatchPipelineBootstrapResult m_pipelineBootstrapResult =
		StaticBatchPipelineBootstrapResult::NotAttempted;
	bool m_lastUploadSucceeded = false;
};
