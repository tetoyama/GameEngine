// =======================================================================
// NullRHIBackend.h
//
// Native Graphics APIを使用しないRHI契約検証用Backend。
// =======================================================================
#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>

#include "Service/Graphics/RHI/RHIBackend.h"
#include "Service/Graphics/RHI/RHIResourcePool.h"

namespace RHI {

class NullRHIDevice;

class NullRHICommandList final : public IRHICommandList {
public:
	explicit NullRHICommandList(CommandQueueType queueType)
		: m_queueType(queueType){}

	CommandQueueType GetQueueType() const noexcept override { return m_queueType; }
	void Begin() override { m_recording = true; m_closed = false; m_renderPassActive = false; m_drawCount = 0; m_dispatchCount = 0; }
	void End() override { if(m_renderPassActive) EndRenderPass(); m_recording = false; m_closed = true; }
	bool ResourceBarrier(std::span<const ResourceBarrierDesc> barriers) override {
		if(!m_recording || m_renderPassActive) return false;
		for(const ResourceBarrierDesc& barrier : barriers){
			if(!barrier.HasExactlyOneResource()) return false;
			if(barrier.type == ResourceBarrierType::Transition &&
				barrier.after == ResourceState::Undefined){
				return false;
			}
			if(barrier.buffer && barrier.subresource != AllSubresources){
				return false;
			}
		}
		return true;
	}
	bool BeginRenderPass(const RenderPassDesc&) override { if(!m_recording || m_queueType != CommandQueueType::Graphics) return false; m_renderPassActive = true; return true; }
	void EndRenderPass() override { m_renderPassActive = false; }
	bool SetPipelineState(PipelineStateHandle pipeline) override { return m_recording && static_cast<bool>(pipeline); }
	void SetViewport(const Viewport&) override {}
	bool SetVertexBuffer(uint32_t, BufferHandle buffer, uint32_t, uint32_t) override { return m_recording && static_cast<bool>(buffer); }
	bool SetIndexBuffer(BufferHandle buffer, IndexFormat, uint32_t) override { return m_recording && static_cast<bool>(buffer); }
	bool SetConstantBuffer(ShaderStage, uint32_t, BufferHandle buffer) override { return m_recording && static_cast<bool>(buffer); }
	bool SetTexture(ShaderStage, uint32_t, TextureHandle texture) override { return m_recording && static_cast<bool>(texture); }
	bool UpdateBuffer(BufferHandle buffer, std::span<const std::byte>, uint32_t) override { return m_recording && static_cast<bool>(buffer); }
	void Draw(uint32_t, uint32_t) override { if(m_recording && m_queueType == CommandQueueType::Graphics) ++m_drawCount; }
	void DrawIndexed(uint32_t, uint32_t, int32_t) override { if(m_recording && m_queueType == CommandQueueType::Graphics) ++m_drawCount; }
	void Dispatch(uint32_t, uint32_t, uint32_t) override { if(m_recording && m_queueType != CommandQueueType::Copy) ++m_dispatchCount; }
	bool IsClosed() const noexcept { return m_closed; }
	uint32_t GetDrawCount() const noexcept { return m_drawCount; }
	uint32_t GetDispatchCount() const noexcept { return m_dispatchCount; }

private:
	CommandQueueType m_queueType = CommandQueueType::Graphics;
	bool m_recording = false;
	bool m_closed = false;
	bool m_renderPassActive = false;
	uint32_t m_drawCount = 0;
	uint32_t m_dispatchCount = 0;
};

struct NullFenceState {
	std::mutex mutex;
	std::condition_variable condition;
	uint64_t completedValue = 0;
};

class NullRHIFence final : public IRHIFence {
public:
	NullRHIFence(FenceHandle handle, std::shared_ptr<NullFenceState> state)
		: m_handle(handle), m_state(std::move(state)){}
	FenceHandle GetHandle() const noexcept override { return m_handle; }
	uint64_t GetCompletedValue() const override { std::scoped_lock lock(m_state->mutex); return m_state->completedValue; }
	bool Wait(uint64_t value, uint64_t timeoutNanoseconds) override {
		std::unique_lock lock(m_state->mutex);
		if(timeoutNanoseconds == 0) return m_state->completedValue >= value;
		return m_state->condition.wait_for(lock, std::chrono::nanoseconds(timeoutNanoseconds), [&]{ return m_state->completedValue >= value; });
	}
private:
	FenceHandle m_handle;
	std::shared_ptr<NullFenceState> m_state;
};

class NullRHICommandQueue final : public IRHICommandQueue {
public:
	NullRHICommandQueue() = default;
	NullRHICommandQueue(NullRHIDevice* owner, CommandQueueType type) : m_owner(owner), m_type(type){}
	CommandQueueType GetType() const noexcept override { return m_type; }
	bool Submit(const QueueSubmitDesc& desc) override;
	void WaitIdle() override {}
private:
	NullRHIDevice* m_owner = nullptr;
	CommandQueueType m_type = CommandQueueType::Graphics;
};

class NullRHISwapChain final : public IRHISwapChain {
public:
	explicit NullRHISwapChain(SwapChainDesc desc) : m_desc(std::move(desc)){}
	bool Resize(uint32_t width, uint32_t height) override { if(width == 0 || height == 0) return false; m_desc.width = width; m_desc.height = height; return true; }
	bool Present(bool) override { ++m_presentCount; return true; }
	const SwapChainDesc& GetDesc() const override { return m_desc; }
	uint64_t GetPresentCount() const noexcept { return m_presentCount; }
private:
	SwapChainDesc m_desc;
	uint64_t m_presentCount = 0;
};

class NullRHIDevice final : public IRHIDevice {
public:
	explicit NullRHIDevice(const DeviceCreateDesc& desc)
		: m_swapChain(desc.swapChain), m_graphicsQueue(this, CommandQueueType::Graphics), m_computeQueue(this, CommandQueueType::Compute), m_copyQueue(this, CommandQueueType::Copy){
		m_capabilities.backend = BackendType::Null;
		m_capabilities.maximumColorAttachments = 8;
		m_capabilities.maximumTextureDimension2D = 16384;
		m_capabilities.maximumConstantBufferSize = 65536;
		m_capabilities.maximumVertexBufferSlots = 16;
		m_capabilities.maximumShaderResourceSlots = 128;
		m_capabilities.supportsCompute = true;
		m_capabilities.supportsIndirectDraw = true;
		m_capabilities.supportsAsyncCompute = true;
		m_capabilities.supportsMultipleCommandQueues = true;
		m_capabilities.supportsTimelineSynchronization = true;
	}

	BackendType GetBackendType() const noexcept override { return BackendType::Null; }
	const DeviceCapabilities& GetCapabilities() const noexcept override { return m_capabilities; }
	BufferHandle CreateBuffer(const BufferDesc& desc, std::span<const std::byte>) override { return desc.byteSize > 0 ? m_buffers.Create(desc) : BufferHandle{}; }
	TextureHandle CreateTexture(const TextureDesc& desc, std::span<const std::byte>, uint32_t) override { return desc.width > 0 && desc.height > 0 ? m_textures.Create(desc) : TextureHandle{}; }
	ShaderHandle CreateShader(const ShaderDesc& desc, std::span<const std::byte> byteCode) override { return !byteCode.empty() ? m_shaders.Create(desc) : ShaderHandle{}; }
	PipelineStateHandle CreatePipelineState(const PipelineStateDesc& desc) override { return m_pipelines.Create(desc); }
	bool DestroyBuffer(BufferHandle handle) override { return m_buffers.Destroy(handle); }
	bool DestroyTexture(TextureHandle handle) override { return m_textures.Destroy(handle); }
	bool DestroyShader(ShaderHandle handle) override { return m_shaders.Destroy(handle); }
	bool DestroyPipelineState(PipelineStateHandle handle) override { return m_pipelines.Destroy(handle); }
	const BufferDesc* GetBufferDesc(BufferHandle handle) const override { return m_buffers.TryGet(handle); }
	const TextureDesc* GetTextureDesc(TextureHandle handle) const override { return m_textures.TryGet(handle); }
	const ShaderDesc* GetShaderDesc(ShaderHandle handle) const override { return m_shaders.TryGet(handle); }
	const PipelineStateDesc* GetPipelineStateDesc(PipelineStateHandle handle) const override { return m_pipelines.TryGet(handle); }
	IRHICommandQueue* GetQueue(CommandQueueType type) override { switch(type){ case CommandQueueType::Graphics: return &m_graphicsQueue; case CommandQueueType::Compute: return &m_computeQueue; case CommandQueueType::Copy: return &m_copyQueue; } return nullptr; }
	const IRHICommandQueue* GetQueue(CommandQueueType type) const override { return const_cast<NullRHIDevice*>(this)->GetQueue(type); }
	std::unique_ptr<IRHICommandList> CreateCommandList(const CommandListCreateDesc& desc) override { return std::make_unique<NullRHICommandList>(desc.queueType); }
	std::unique_ptr<IRHIFence> CreateFence(uint64_t initialValue) override { auto state = std::make_shared<NullFenceState>(); state->completedValue = initialValue; const FenceHandle handle = m_fences.Create(state); return std::make_unique<NullRHIFence>(handle, std::move(state)); }
	IRHISwapChain* GetSwapChain() override { return &m_swapChain; }
	const IRHISwapChain* GetSwapChain() const override { return &m_swapChain; }
	void WaitIdle() override {}
	std::shared_ptr<NullFenceState> FindFence(FenceHandle handle) const { const auto* state = m_fences.TryGet(handle); return state ? *state : nullptr; }

private:
	DeviceCapabilities m_capabilities;
	NullRHISwapChain m_swapChain;
	NullRHICommandQueue m_graphicsQueue;
	NullRHICommandQueue m_computeQueue;
	NullRHICommandQueue m_copyQueue;
	ResourcePool<BufferHandle, BufferDesc> m_buffers;
	ResourcePool<TextureHandle, TextureDesc> m_textures;
	ResourcePool<ShaderHandle, ShaderDesc> m_shaders;
	ResourcePool<PipelineStateHandle, PipelineStateDesc> m_pipelines;
	ResourcePool<FenceHandle, std::shared_ptr<NullFenceState>> m_fences;
};

inline bool NullRHICommandQueue::Submit(const QueueSubmitDesc& desc){
	if(!m_owner) return false;
	if(desc.waitFence){ auto state = m_owner->FindFence(desc.waitFence); if(!state) return false; std::unique_lock lock(state->mutex); state->condition.wait(lock, [&]{ return state->completedValue >= desc.waitValue; }); }
	for(IRHICommandList* commandList : desc.commandLists){ if(!commandList || commandList->GetQueueType() != m_type) return false; auto* nullList = dynamic_cast<NullRHICommandList*>(commandList); if(!nullList || !nullList->IsClosed()) return false; }
	if(desc.signalFence){ auto state = m_owner->FindFence(desc.signalFence); if(!state) return false; { std::scoped_lock lock(state->mutex); state->completedValue = (std::max)(state->completedValue, desc.signalValue); } state->condition.notify_all(); }
	return true;
}

class NullRHIBackend final : public IRHIBackend {
public:
	BackendType GetType() const noexcept override { return BackendType::Null; }
	std::string_view GetName() const noexcept override { return "Null"; }
	bool IsSupported() const override { return true; }
	std::vector<AdapterInfo> EnumerateAdapters() const override { return {AdapterInfo{0, "Null Adapter", 0, 0, true}}; }
	std::unique_ptr<IRHIDevice> CreateDevice(const DeviceCreateDesc& desc) override { return std::make_unique<NullRHIDevice>(desc); }
};

inline std::unique_ptr<IRHIBackend> CreateNullRHIBackend(){ return std::make_unique<NullRHIBackend>(); }
inline bool RegisterNullRHIBackend(){ return BackendRegistry::Instance().Register(BackendType::Null, &CreateNullRHIBackend); }

} // namespace RHI
