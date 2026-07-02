#pragma once

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include "Service/Graphics/RHI/RHIInterfaces.h"
#include "Service/Graphics/RHI/RHIResourcePool.h"
#include "D3D11RHIResources.h"

namespace RHI {

class D3D11RHIDevice;

class D3D11RHISwapChain final : public IRHISwapChain {
public:
	D3D11RHISwapChain() = default;
	D3D11RHISwapChain(D3D11RHIDevice*, IDXGISwapChain*, const SwapChainDesc&);
	void Attach(D3D11RHIDevice*, IDXGISwapChain*, const SwapChainDesc&);
	bool Resize(uint32_t, uint32_t) override;
	bool Present(bool) override;
	const SwapChainDesc& GetDesc() const override { return m_desc; }
	uint32_t GetImageCount() const noexcept override { return static_cast<uint32_t>(m_images.size()); }
	uint32_t GetCurrentImageIndex() const noexcept override;
	TextureHandle GetImage(uint32_t i) const noexcept override { return i < m_images.size() ? m_images[i] : TextureHandle{}; }
	CommandQueueType GetPresentQueueType() const noexcept override { return CommandQueueType::Graphics; }
	IDXGISwapChain* NativeSwapChain() const noexcept { return m_swapChain.Get(); }
private:
	friend class D3D11RHIDevice;
	D3D11RHIDevice* m_owner = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
	SwapChainDesc m_desc;
	std::vector<TextureHandle> m_images;
};

class D3D11RHICommandList final : public IRHICommandList {
public:
	D3D11RHICommandList() = default;
	D3D11RHICommandList(D3D11RHIDevice*, CommandQueueType = CommandQueueType::Graphics);
	void Attach(D3D11RHIDevice*, CommandQueueType);
	CommandQueueType GetQueueType() const noexcept override { return m_queueType; }
	void Begin() override;
	void End() override;
	bool ResourceBarrier(std::span<const ResourceBarrierDesc>) override;
	bool BeginRenderPass(const RenderPassDesc&) override;
	void EndRenderPass() override;
	bool SetPipelineState(PipelineStateHandle) override;
	void SetViewport(const Viewport&) override;
	bool SetVertexBuffer(uint32_t, BufferHandle, uint32_t, uint32_t) override;
	bool SetIndexBuffer(BufferHandle, IndexFormat, uint32_t) override;
	bool SetConstantBuffer(ShaderStage, uint32_t, BufferHandle) override;
	bool SetBufferView(ShaderStage, uint32_t, BufferViewHandle) override;
	bool SetTextureView(ShaderStage, uint32_t, TextureViewHandle) override;
	bool SetSampler(ShaderStage, uint32_t, SamplerHandle) override;
	bool UpdateBuffer(BufferHandle, std::span<const std::byte>, uint32_t) override;
	void Draw(uint32_t, uint32_t) override;
	void DrawIndexed(uint32_t, uint32_t, int32_t) override;
	bool DrawIndexedInstanced(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override;
	void Dispatch(uint32_t, uint32_t, uint32_t) override;
	bool IsClosed() const noexcept { return m_closed; }
private:
	D3D11RHIDevice* m_owner = nullptr;
	CommandQueueType m_queueType = CommandQueueType::Graphics;
	bool m_recording = false;
	bool m_closed = false;
	bool m_renderPassActive = false;
};

struct D3D11FenceSignal {
	uint64_t value = 0;
	Microsoft::WRL::ComPtr<ID3D11Query> query;
};

struct D3D11FenceState {
	std::mutex mutex;
	uint64_t completedValue = 0;
	uint64_t lastSignaledValue = 0;
	std::deque<D3D11FenceSignal> pendingSignals;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	uint64_t PollCompletedValue();
	bool Wait(uint64_t, uint64_t);
};

class D3D11RHIFence final : public IRHIFence {
public:
	D3D11RHIFence(FenceHandle h, std::shared_ptr<D3D11FenceState> s) : m_handle(h), m_state(std::move(s)){}
	FenceHandle GetHandle() const noexcept override { return m_handle; }
	uint64_t GetCompletedValue() const override;
	bool Wait(uint64_t, uint64_t) override;
private:
	FenceHandle m_handle;
	std::shared_ptr<D3D11FenceState> m_state;
};

class D3D11RHICommandQueue final : public IRHICommandQueue {
public:
	D3D11RHICommandQueue() = default;
	D3D11RHICommandQueue(D3D11RHIDevice* o, CommandQueueType t) : m_owner(o), m_type(t){}
	void Attach(D3D11RHIDevice* o, CommandQueueType t){ m_owner = o; m_type = t; }
	CommandQueueType GetType() const noexcept override { return m_type; }
	bool Submit(const QueueSubmitDesc&) override;
	bool Present(IRHISwapChain&, bool) override;
	void WaitIdle() override;
private:
	D3D11RHIDevice* m_owner = nullptr;
	CommandQueueType m_type = CommandQueueType::Graphics;
};

class D3D11RHIDevice final : public IRHIDevice {
public:
	D3D11RHIDevice(ID3D11Device*, ID3D11DeviceContext*, IDXGISwapChain*, const SwapChainDesc&);
	~D3D11RHIDevice() override;
	BackendType GetBackendType() const noexcept override { return BackendType::Direct3D11; }
	const DeviceCapabilities& GetCapabilities() const noexcept override { return m_capabilities; }
	BufferHandle CreateBuffer(const BufferDesc&, std::span<const std::byte>) override;
	BufferHandle ImportNativeBuffer(
		ID3D11Buffer*,
		uint32_t stride,
		ResourceState initialState,
		const char* debugName = nullptr
	);
	TextureHandle CreateTexture(const TextureDesc&, std::span<const std::byte>, uint32_t) override;
	BufferViewHandle CreateBufferView(const BufferViewDesc&) override;
	TextureViewHandle CreateTextureView(const TextureViewDesc&) override;
	SamplerHandle CreateSampler(const SamplerDesc&) override;
	ShaderHandle CreateShader(const ShaderDesc&, std::span<const std::byte>) override;
	PipelineStateHandle CreatePipelineState(const PipelineStateDesc&) override;
	bool DestroyBuffer(BufferHandle) override;
	bool DestroyTexture(TextureHandle) override;
	bool DestroyBufferView(BufferViewHandle) override;
	bool DestroyTextureView(TextureViewHandle) override;
	bool DestroySampler(SamplerHandle) override;
	bool DestroyShader(ShaderHandle) override;
	bool DestroyPipelineState(PipelineStateHandle) override;
	const BufferDesc* GetBufferDesc(BufferHandle) const override;
	const TextureDesc* GetTextureDesc(TextureHandle) const override;
	const BufferViewDesc* GetBufferViewDesc(BufferViewHandle) const override;
	const TextureViewDesc* GetTextureViewDesc(TextureViewHandle) const override;
	const SamplerDesc* GetSamplerDesc(SamplerHandle) const override;
	const ShaderDesc* GetShaderDesc(ShaderHandle) const override;
	const PipelineStateDesc* GetPipelineStateDesc(PipelineStateHandle) const override;
	IRHICommandQueue* GetQueue(CommandQueueType) override;
	const IRHICommandQueue* GetQueue(CommandQueueType) const override;
	std::unique_ptr<IRHICommandList> CreateCommandList(const CommandListCreateDesc&) override;
	std::unique_ptr<IRHIFence> CreateFence(uint64_t) override;
	bool DestroyFence(FenceHandle) override;
	IRHISwapChain* GetSwapChain() override { return &m_swapChain; }
	const IRHISwapChain* GetSwapChain() const override { return &m_swapChain; }
	void WaitIdle() override;
	ID3D11Device* NativeDevice() const noexcept { return m_device.Get(); }
	ID3D11DeviceContext* NativeContext() const noexcept { return m_context.Get(); }
	IDXGISwapChain* NativeSwapChain() const noexcept { return m_swapChain.NativeSwapChain(); }
private:
	friend class D3D11RHICommandList;
	friend class D3D11RHICommandQueue;
	friend class D3D11RHISwapChain;
	D3D11BufferResource* Find(BufferHandle h){ return m_buffers.TryGet(h); }
	D3D11TextureResource* Find(TextureHandle h){ return m_textures.TryGet(h); }
	D3D11BufferViewResource* Find(BufferViewHandle h){ return m_bufferViews.TryGet(h); }
	D3D11TextureViewResource* Find(TextureViewHandle h){ return m_textureViews.TryGet(h); }
	D3D11SamplerResource* Find(SamplerHandle h){ return m_samplers.TryGet(h); }
	D3D11ShaderResource* Find(ShaderHandle h){ return m_shaders.TryGet(h); }
	D3D11PipelineStateResource* Find(PipelineStateHandle h){ return m_pipelines.TryGet(h); }
	const D3D11BufferResource* Find(BufferHandle h) const { return m_buffers.TryGet(h); }
	const D3D11TextureResource* Find(TextureHandle h) const { return m_textures.TryGet(h); }
	const D3D11BufferViewResource* Find(BufferViewHandle h) const { return m_bufferViews.TryGet(h); }
	const D3D11TextureViewResource* Find(TextureViewHandle h) const { return m_textureViews.TryGet(h); }
	const D3D11SamplerResource* Find(SamplerHandle h) const { return m_samplers.TryGet(h); }
	const D3D11ShaderResource* Find(ShaderHandle h) const { return m_shaders.TryGet(h); }
	const D3D11PipelineStateResource* Find(PipelineStateHandle h) const { return m_pipelines.TryGet(h); }
	std::shared_ptr<D3D11FenceState> FindFence(FenceHandle) const;
	bool SignalFence(const std::shared_ptr<D3D11FenceState>&, uint64_t);
	bool WaitForImmediateContext();
	bool ImportSwapChainImages();
	bool ReleaseSwapChainImages();
	Microsoft::WRL::ComPtr<ID3D11Device> m_device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
	DeviceCapabilities m_capabilities;
	D3D11RHISwapChain m_swapChain;
	D3D11RHICommandQueue m_graphicsQueue, m_computeQueue, m_copyQueue;
	ResourcePool<BufferHandle, D3D11BufferResource> m_buffers;
	ResourcePool<TextureHandle, D3D11TextureResource> m_textures;
	ResourcePool<BufferViewHandle, D3D11BufferViewResource> m_bufferViews;
	ResourcePool<TextureViewHandle, D3D11TextureViewResource> m_textureViews;
	ResourcePool<SamplerHandle, D3D11SamplerResource> m_samplers;
	ResourcePool<ShaderHandle, D3D11ShaderResource> m_shaders;
	ResourcePool<PipelineStateHandle, D3D11PipelineStateResource> m_pipelines;
	ResourcePool<FenceHandle, std::shared_ptr<D3D11FenceState>> m_fences;
};

} // namespace RHI

#include "D3D11RHIDevice.inl"
