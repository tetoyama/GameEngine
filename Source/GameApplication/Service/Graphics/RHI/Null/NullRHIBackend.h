#pragma once

#include <algorithm>
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
	explicit NullRHICommandList(CommandQueueType type) : m_queueType(type){}
	CommandQueueType GetQueueType() const noexcept override { return m_queueType; }
	void Begin() override { m_recording = true; m_closed = false; m_renderPassActive = false; m_drawCount = 0; m_dispatchCount = 0; }
	void End() override { if(m_renderPassActive) EndRenderPass(); m_recording = false; m_closed = true; }
	bool ResourceBarrier(std::span<const ResourceBarrierDesc> barriers) override {
		if(!m_recording || m_renderPassActive) return false;
		for(const auto& barrier : barriers){
			if(!barrier.HasExactlyOneResource()) return false;
			if(barrier.type == ResourceBarrierType::Transition && barrier.after == ResourceState::Undefined) return false;
			if(barrier.buffer && barrier.subresource != AllSubresources) return false;
		}
		return true;
	}
	bool BeginRenderPass(const RenderPassDesc&) override { if(!m_recording || m_queueType != CommandQueueType::Graphics) return false; m_renderPassActive = true; return true; }
	void EndRenderPass() override { m_renderPassActive = false; }
	bool SetPipelineState(PipelineStateHandle h) override { return m_recording && m_queueType != CommandQueueType::Copy && static_cast<bool>(h); }
	void SetViewport(const Viewport&) override {}
	bool SetVertexBuffer(uint32_t, BufferHandle h, uint32_t, uint32_t) override { return m_recording && m_queueType == CommandQueueType::Graphics && static_cast<bool>(h); }
	bool SetIndexBuffer(BufferHandle h, IndexFormat, uint32_t) override { return m_recording && m_queueType == CommandQueueType::Graphics && static_cast<bool>(h); }
	bool SetConstantBuffer(ShaderStage s, uint32_t, BufferHandle h) override { return CanBind(s) && static_cast<bool>(h); }
	bool SetBufferView(ShaderStage s, uint32_t, BufferViewHandle h) override { return CanBind(s) && static_cast<bool>(h); }
	bool SetTextureView(ShaderStage s, uint32_t, TextureViewHandle h) override { return CanBind(s) && static_cast<bool>(h); }
	bool SetSampler(ShaderStage s, uint32_t, SamplerHandle h) override { return CanBind(s) && static_cast<bool>(h); }
	bool UpdateBuffer(BufferHandle h, std::span<const std::byte>, uint32_t) override { return m_recording && static_cast<bool>(h); }
	void Draw(uint32_t, uint32_t) override { if(m_recording && m_queueType == CommandQueueType::Graphics) ++m_drawCount; }
	void DrawIndexed(uint32_t, uint32_t, int32_t) override { if(m_recording && m_queueType == CommandQueueType::Graphics) ++m_drawCount; }
	void Dispatch(uint32_t, uint32_t, uint32_t) override { if(m_recording && m_queueType != CommandQueueType::Copy) ++m_dispatchCount; }
	bool IsClosed() const noexcept { return m_closed; }
private:
	bool CanBind(ShaderStage stage) const { return m_recording && m_queueType != CommandQueueType::Copy && (m_queueType != CommandQueueType::Compute || stage == ShaderStage::Compute); }
	CommandQueueType m_queueType = CommandQueueType::Graphics;
	bool m_recording = false;
	bool m_closed = false;
	bool m_renderPassActive = false;
	uint32_t m_drawCount = 0;
	uint32_t m_dispatchCount = 0;
};

struct NullFenceState { std::mutex mutex; std::condition_variable condition; uint64_t completedValue = 0; };

class NullRHIFence final : public IRHIFence {
public:
	NullRHIFence(FenceHandle h, std::shared_ptr<NullFenceState> s) : m_handle(h), m_state(std::move(s)){}
	FenceHandle GetHandle() const noexcept override { return m_handle; }
	uint64_t GetCompletedValue() const override { std::scoped_lock lock(m_state->mutex); return m_state->completedValue; }
	bool Wait(uint64_t value, uint64_t timeout) override {
		std::unique_lock lock(m_state->mutex);
		if(timeout == 0) return m_state->completedValue >= value;
		return m_state->condition.wait_for(lock, std::chrono::nanoseconds(timeout), [&]{ return m_state->completedValue >= value; });
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
	bool Submit(const QueueSubmitDesc&) override;
	bool Present(IRHISwapChain&, bool) override;
	void WaitIdle() override {}
private:
	NullRHIDevice* m_owner = nullptr;
	CommandQueueType m_type = CommandQueueType::Graphics;
};

class NullRHISwapChain final : public IRHISwapChain {
public:
	NullRHISwapChain(NullRHIDevice* owner, SwapChainDesc desc) : m_owner(owner), m_desc(std::move(desc)){}
	bool Resize(uint32_t, uint32_t) override;
	bool Present(bool) override { ++m_presentCount; return true; }
	const SwapChainDesc& GetDesc() const override { return m_desc; }
	uint32_t GetImageCount() const noexcept override { return m_image ? 1u : 0u; }
	uint32_t GetCurrentImageIndex() const noexcept override { return 0; }
	TextureHandle GetImage(uint32_t index) const noexcept override { return index == 0 ? m_image : TextureHandle{}; }
	CommandQueueType GetPresentQueueType() const noexcept override { return CommandQueueType::Graphics; }
	void SetImage(TextureHandle image) noexcept { m_image = image; }
private:
	NullRHIDevice* m_owner = nullptr;
	SwapChainDesc m_desc;
	TextureHandle m_image;
	uint64_t m_presentCount = 0;
};

struct NullBufferResource { BufferDesc desc; uint32_t viewCount = 0; };
struct NullTextureResource { TextureDesc desc; uint32_t viewCount = 0; bool swapChainOwned = false; };

class NullRHIDevice final : public IRHIDevice {
public:
	explicit NullRHIDevice(const DeviceCreateDesc& desc)
		: m_swapChain(this, desc.swapChain), m_graphicsQueue(this, CommandQueueType::Graphics), m_computeQueue(this, CommandQueueType::Compute), m_copyQueue(this, CommandQueueType::Copy){
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
		RecreateSwapChainImage(desc.swapChain.width, desc.swapChain.height);
	}
	BackendType GetBackendType() const noexcept override { return BackendType::Null; }
	const DeviceCapabilities& GetCapabilities() const noexcept override { return m_capabilities; }
	BufferHandle CreateBuffer(const BufferDesc& d, std::span<const std::byte>) override { return d.byteSize ? m_buffers.Create(NullBufferResource{d}) : BufferHandle{}; }
	TextureHandle CreateTexture(const TextureDesc& d, std::span<const std::byte>, uint32_t) override { return d.width && d.height ? m_textures.Create(NullTextureResource{d}) : TextureHandle{}; }
	BufferViewHandle CreateBufferView(const BufferViewDesc& d) override {
		auto* r = m_buffers.TryGet(d.buffer); if(!r) return {};
		const auto flag = d.type == BufferViewType::ShaderResource ? BufferBindFlags::ShaderResource : BufferBindFlags::UnorderedAccess;
		if(!HasAnyFlag(r->desc.bindFlags, flag)) return {};
		++r->viewCount; return m_bufferViews.Create(d);
	}
	TextureViewHandle CreateTextureView(const TextureViewDesc& d) override {
		auto* r = m_textures.TryGet(d.texture); if(!r || d.baseMipLevel >= r->desc.mipLevels || d.mipLevelCount == 0 || d.mipLevelCount > r->desc.mipLevels - d.baseMipLevel) return {};
		TextureBindFlags flag = TextureBindFlags::ShaderResource;
		if(d.type == TextureViewType::UnorderedAccess) flag = TextureBindFlags::UnorderedAccess;
		else if(d.type == TextureViewType::RenderTarget) flag = TextureBindFlags::RenderTarget;
		else if(d.type == TextureViewType::DepthStencil) flag = TextureBindFlags::DepthStencil;
		if(!HasAnyFlag(r->desc.bindFlags, flag)) return {};
		++r->viewCount; return m_textureViews.Create(d);
	}
	SamplerHandle CreateSampler(const SamplerDesc& d) override { return d.minLod <= d.maxLod && (!d.anisotropyEnable || (d.maxAnisotropy > 0 && d.maxAnisotropy <= 16)) ? m_samplers.Create(d) : SamplerHandle{}; }
	ShaderHandle CreateShader(const ShaderDesc& d, std::span<const std::byte> code) override { return !code.empty() ? m_shaders.Create(d) : ShaderHandle{}; }
	PipelineStateHandle CreatePipelineState(const PipelineStateDesc& d) override { return m_pipelines.Create(d); }
	bool DestroyBuffer(BufferHandle h) override { auto* r = m_buffers.TryGet(h); return r && r->viewCount == 0 && m_buffers.Destroy(h); }
	bool DestroyTexture(TextureHandle h) override { auto* r = m_textures.TryGet(h); return r && !r->swapChainOwned && r->viewCount == 0 && m_textures.Destroy(h); }
	bool DestroyBufferView(BufferViewHandle h) override { auto* v = m_bufferViews.TryGet(h); if(!v) return false; if(auto* r = m_buffers.TryGet(v->buffer); r && r->viewCount) --r->viewCount; return m_bufferViews.Destroy(h); }
	bool DestroyTextureView(TextureViewHandle h) override { auto* v = m_textureViews.TryGet(h); if(!v) return false; if(auto* r = m_textures.TryGet(v->texture); r && r->viewCount) --r->viewCount; return m_textureViews.Destroy(h); }
	bool DestroySampler(SamplerHandle h) override { return m_samplers.Destroy(h); }
	bool DestroyShader(ShaderHandle h) override { return m_shaders.Destroy(h); }
	bool DestroyPipelineState(PipelineStateHandle h) override { return m_pipelines.Destroy(h); }
	const BufferDesc* GetBufferDesc(BufferHandle h) const override { auto* r = m_buffers.TryGet(h); return r ? &r->desc : nullptr; }
	const TextureDesc* GetTextureDesc(TextureHandle h) const override { auto* r = m_textures.TryGet(h); return r ? &r->desc : nullptr; }
	const BufferViewDesc* GetBufferViewDesc(BufferViewHandle h) const override { return m_bufferViews.TryGet(h); }
	const TextureViewDesc* GetTextureViewDesc(TextureViewHandle h) const override { return m_textureViews.TryGet(h); }
	const SamplerDesc* GetSamplerDesc(SamplerHandle h) const override { return m_samplers.TryGet(h); }
	const ShaderDesc* GetShaderDesc(ShaderHandle h) const override { return m_shaders.TryGet(h); }
	const PipelineStateDesc* GetPipelineStateDesc(PipelineStateHandle h) const override { return m_pipelines.TryGet(h); }
	IRHICommandQueue* GetQueue(CommandQueueType t) override { switch(t){ case CommandQueueType::Graphics: return &m_graphicsQueue; case CommandQueueType::Compute: return &m_computeQueue; case CommandQueueType::Copy: return &m_copyQueue; } return nullptr; }
	const IRHICommandQueue* GetQueue(CommandQueueType t) const override { return const_cast<NullRHIDevice*>(this)->GetQueue(t); }
	std::unique_ptr<IRHICommandList> CreateCommandList(const CommandListCreateDesc& d) override { return d.secondary ? nullptr : std::make_unique<NullRHICommandList>(d.queueType); }
	std::unique_ptr<IRHIFence> CreateFence(uint64_t value) override { auto state = std::make_shared<NullFenceState>(); state->completedValue = value; auto handle = m_fences.Create(state); return std::make_unique<NullRHIFence>(handle, std::move(state)); }
	IRHISwapChain* GetSwapChain() override { return &m_swapChain; }
	const IRHISwapChain* GetSwapChain() const override { return &m_swapChain; }
	void WaitIdle() override {}
	std::shared_ptr<NullFenceState> FindFence(FenceHandle h) const { auto* s = m_fences.TryGet(h); return s ? *s : nullptr; }
	bool RecreateSwapChainImage(uint32_t width, uint32_t height){
		if(width == 0 || height == 0) return false;
		if(m_swapChainImage){
			auto* current = m_textures.TryGet(m_swapChainImage);
			if(!current || current->viewCount != 0) return false;
			m_textures.Destroy(m_swapChainImage);
		}
		TextureDesc imageDesc;
		imageDesc.width = width;
		imageDesc.height = height;
		imageDesc.format = m_swapChain.GetDesc().format;
		imageDesc.bindFlags = TextureBindFlags::RenderTarget;
		imageDesc.initialState = ResourceState::Present;
		imageDesc.debugName = "Null SwapChain Image";
		m_swapChainImage = m_textures.Create(NullTextureResource{imageDesc, 0, true});
		m_swapChain.SetImage(m_swapChainImage);
		return static_cast<bool>(m_swapChainImage);
	}
private:
	DeviceCapabilities m_capabilities;
	NullRHISwapChain m_swapChain;
	NullRHICommandQueue m_graphicsQueue, m_computeQueue, m_copyQueue;
	TextureHandle m_swapChainImage;
	ResourcePool<BufferHandle, NullBufferResource> m_buffers;
	ResourcePool<TextureHandle, NullTextureResource> m_textures;
	ResourcePool<BufferViewHandle, BufferViewDesc> m_bufferViews;
	ResourcePool<TextureViewHandle, TextureViewDesc> m_textureViews;
	ResourcePool<SamplerHandle, SamplerDesc> m_samplers;
	ResourcePool<ShaderHandle, ShaderDesc> m_shaders;
	ResourcePool<PipelineStateHandle, PipelineStateDesc> m_pipelines;
	ResourcePool<FenceHandle, std::shared_ptr<NullFenceState>> m_fences;
};

inline bool NullRHISwapChain::Resize(uint32_t width, uint32_t height){
	if(!m_owner || !m_owner->RecreateSwapChainImage(width, height)) return false;
	m_desc.width = width;
	m_desc.height = height;
	return true;
}

inline bool NullRHICommandQueue::Submit(const QueueSubmitDesc& d){
	if(!m_owner) return false;
	if(d.waitFence){ auto s = m_owner->FindFence(d.waitFence); if(!s) return false; std::unique_lock lock(s->mutex); s->condition.wait(lock, [&]{ return s->completedValue >= d.waitValue; }); }
	for(auto* list : d.commandLists){ if(!list || list->GetQueueType() != m_type) return false; auto* n = dynamic_cast<NullRHICommandList*>(list); if(!n || !n->IsClosed()) return false; }
	if(d.signalFence){ auto s = m_owner->FindFence(d.signalFence); if(!s) return false; { std::scoped_lock lock(s->mutex); s->completedValue = (std::max)(s->completedValue, d.signalValue); } s->condition.notify_all(); }
	return true;
}

inline bool NullRHICommandQueue::Present(IRHISwapChain& swapChain, bool verticalSync){
	if(m_type != CommandQueueType::Graphics) return false;
	auto* nullSwapChain = dynamic_cast<NullRHISwapChain*>(&swapChain);
	return nullSwapChain && nullSwapChain->Present(verticalSync);
}

class NullRHIBackend final : public IRHIBackend {
public:
	BackendType GetType() const noexcept override { return BackendType::Null; }
	std::string_view GetName() const noexcept override { return "Null"; }
	bool IsSupported() const override { return true; }
	std::vector<AdapterInfo> EnumerateAdapters() const override { return {AdapterInfo{0, "Null Adapter", 0, 0, true}}; }
	std::unique_ptr<IRHIDevice> CreateDevice(const DeviceCreateDesc& d) override { return std::make_unique<NullRHIDevice>(d); }
};

inline std::unique_ptr<IRHIBackend> CreateNullRHIBackend(){ return std::make_unique<NullRHIBackend>(); }
inline bool RegisterNullRHIBackend(){ return BackendRegistry::Instance().Register(BackendType::Null, &CreateNullRHIBackend); }

} // namespace RHI
