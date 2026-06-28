#pragma once

#include <array>
#include <memory>

#include "Interface/ISystem.h"
#include "Graphics/graphicsContext.h"
#include "Graphics/RHI/RHIService.h"
#include "Registry/systemRegistry.h"
#include "Scene/sceneManager.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchGpuInstanceBuffer.h"

class StaticBatchUploadSystem final : public ISystem {
public:
	explicit StaticBatchUploadSystem(SceneManagerContext* context)
		: m_context(context) {
	}

	const char* GetSystemName() const override {
		return "StaticBatchUploadSystem";
	}

	void Finalize() override {
		RHI::IRHIDevice* device = ResolveDevice();
		if(device){
			m_gpuInstanceBuffer.Release(*device);
		}
		m_lastUploadSucceeded = false;
	}

	void Stop() override {
		m_lastUploadSucceeded = false;
	}

	void RegisterTasks(SystemScheduleBuilder& builder) override {
		SystemAccess access;
		access
			.ReadResource<RenderPacketFrameBuffer>()
			.WriteResource<StaticBatchGpuInstanceBuffer>();

		builder.AddTask(
			"StaticBatchUploadSystem.Instance.Upload",
			SystemTaskDomain::Render,
			SystemPhase::Default,
			0,
			std::move(access),
			ThreadAffinity::MainThread,
			[this](const SystemTaskContext&){
				Upload();
			}
		);
	}

	const StaticBatchGpuInstanceBuffer& GetGpuInstanceBuffer() const noexcept {
		return m_gpuInstanceBuffer;
	}

	StaticBatchGpuInstanceBufferTelemetry GetTelemetry() const noexcept {
		return m_gpuInstanceBuffer.Telemetry();
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

	void Upload(){
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
	bool m_lastUploadSucceeded = false;
};
