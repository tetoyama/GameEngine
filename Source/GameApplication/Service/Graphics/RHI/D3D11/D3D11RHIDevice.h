// =======================================================================
// D3D11RHIDevice.h
// =======================================================================
#pragma once

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>

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
	D3D11RHISwapChain(IDXGISwapChain* swapChain, const SwapChainDesc& desc);

	void Attach(IDXGISwapChain* swapChain, const SwapChainDesc& desc);
	bool Resize(uint32_t width, uint32_t height) override;
	bool Present(bool verticalSync) override;
	const SwapChainDesc& GetDesc() const override { return m_desc; }

private:
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
	SwapChainDesc m_desc;
};

class D3D11RHICommandList final : public IRHICommandList {
public:
	D3D11RHICommandList() = default;
	D3D11RHICommandList(
		D3D11RHIDevice* owner,
		CommandQueueType queueType = CommandQueueType::Graphics
	);

	void Attach(D3D11RHIDevice* owner, CommandQueueType queueType);
	CommandQueueType GetQueueType() const noexcept override { return m_queueType; }
	void Begin() override;
	void End() override;
	bool BeginRenderPass(const RenderPassDesc& desc) override;
	void EndRenderPass() override;
	bool SetPipelineState(PipelineStateHandle pipeline) override;
	void SetViewport(const Viewport& viewport) override;
	bool SetVertexBuffer(uint32_t slot, BufferHandle buffer, uint32_t stride, uint32_t offset) override;
	bool SetIndexBuffer(BufferHandle buffer, IndexFormat format, uint32_t offset) override;
	bool SetConstantBuffer(ShaderStage stage, uint32_t slot, BufferHandle buffer) override;
	bool SetTexture(ShaderStage stage, uint32_t slot, TextureHandle texture) override;
	bool UpdateBuffer(BufferHandle buffer, std::span<const std::byte> data, uint32_t destinationOffset) override;
	void Draw(uint32_t vertexCount, uint32_t firstVertex) override;
	void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
	void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

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

// D3D11には公開Timeline Fenceがないため、EVENT Query列でQueue完了値を模擬する。
// Flush完了ではなく、GPUがQueryへ到達した時点でcompletedValueを進める。
struct D3D11FenceState {
	std::mutex mutex;
	uint64_t completedValue = 0;
	uint64_t lastSignaledValue = 0;
	std::deque<D3D11FenceSignal> pendingSignals;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;

	uint64_t PollCompletedValue();
	bool Wait(uint64_t value, uint64_t timeoutNanoseconds);
};

class D3D11RHIFence final : public IRHIFence {
public:
	D3D11RHIFence(FenceHandle handle, std::shared_ptr<D3D11FenceState> state)
		: m_handle(handle), m_state(std::move(state)){}

	FenceHandle GetHandle() const noexcept override { return m_handle; }
	uint64_t GetCompletedValue() const override;
	bool Wait(uint64_t value, uint64_t timeoutNanoseconds) override;

private:
	FenceHandle m_handle;
	std::shared_ptr<D3D11FenceState> m_state;
};

// D3D11はNative Queueを一つしか持たない。
// Graphics / Compute / Copyは論理Queueとして公開し、すべてImmediate Contextへ直列化する。
class D3D11RHICommandQueue final : public IRHICommandQueue {
public:
	D3D11RHICommandQueue() = default;
	D3D11RHICommandQueue(
		D3D11RHIDevice* owner,
		CommandQueueType type
	)
		: m_owner(owner)
		, m_type(type){}

	void Attach(D3D11RHIDevice* owner, CommandQueueType type){
		m_owner = owner;
		m_type = type;
	}
	CommandQueueType GetType() const noexcept override { return m_type; }
	bool Submit(const QueueSubmitDesc& desc) override;
	void WaitIdle() override;

private:
	D3D11RHIDevice* m_owner = nullptr;
	CommandQueueType m_type = CommandQueueType::Graphics;
};

class D3D11RHIDevice final : public IRHIDevice {
public:
	D3D11RHIDevice(
		ID3D11Device* device,
		ID3D11DeviceContext* context,
		IDXGISwapChain* swapChain,
		const SwapChainDesc& swapChainDesc
	);
	~D3D11RHIDevice() override;

	BackendType GetBackendType() const noexcept override {
		return BackendType::Direct3D11;
	}
	const DeviceCapabilities& GetCapabilities() const noexcept override {
		return m_capabilities;
	}

	BufferHandle CreateBuffer(const BufferDesc& desc, std::span<const std::byte> initialData) override;
	TextureHandle CreateTexture(const TextureDesc& desc, std::span<const std::byte> initialData, uint32_t initialRowPitch) override;
	ShaderHandle CreateShader(const ShaderDesc& desc, std::span<const std::byte> byteCode) override;
	PipelineStateHandle CreatePipelineState(const PipelineStateDesc& desc) override;

	bool DestroyBuffer(BufferHandle handle) override;
	bool DestroyTexture(TextureHandle handle) override;
	bool DestroyShader(ShaderHandle handle) override;
	bool DestroyPipelineState(PipelineStateHandle handle) override;

	const BufferDesc* GetBufferDesc(BufferHandle handle) const override;
	const TextureDesc* GetTextureDesc(TextureHandle handle) const override;
	const ShaderDesc* GetShaderDesc(ShaderHandle handle) const override;
	const PipelineStateDesc* GetPipelineStateDesc(PipelineStateHandle handle) const override;

	IRHICommandQueue* GetQueue(CommandQueueType type) override;
	const IRHICommandQueue* GetQueue(CommandQueueType type) const override;
	std::unique_ptr<IRHICommandList> CreateCommandList(const CommandListCreateDesc& desc) override;
	std::unique_ptr<IRHIFence> CreateFence(uint64_t initialValue) override;

	IRHISwapChain* GetSwapChain() override { return &m_swapChain; }
	const IRHISwapChain* GetSwapChain() const override { return &m_swapChain; }
	void WaitIdle() override;

	ID3D11Device* NativeDevice() const { return m_device.Get(); }
	ID3D11DeviceContext* NativeContext() const { return m_context.Get(); }

private:
	friend class D3D11RHICommandList;
	friend class D3D11RHICommandQueue;

	D3D11BufferResource* Find(BufferHandle handle) { return m_buffers.TryGet(handle); }
	D3D11TextureResource* Find(TextureHandle handle) { return m_textures.TryGet(handle); }
	D3D11ShaderResource* Find(ShaderHandle handle) { return m_shaders.TryGet(handle); }
	D3D11PipelineStateResource* Find(PipelineStateHandle handle) { return m_pipelines.TryGet(handle); }
	const D3D11BufferResource* Find(BufferHandle handle) const { return m_buffers.TryGet(handle); }
	const D3D11TextureResource* Find(TextureHandle handle) const { return m_textures.TryGet(handle); }
	const D3D11ShaderResource* Find(ShaderHandle handle) const { return m_shaders.TryGet(handle); }
	const D3D11PipelineStateResource* Find(PipelineStateHandle handle) const { return m_pipelines.TryGet(handle); }
	std::shared_ptr<D3D11FenceState> FindFence(FenceHandle handle) const;
	bool SignalFence(const std::shared_ptr<D3D11FenceState>& state, uint64_t value);
	bool WaitForImmediateContext();

	Microsoft::WRL::ComPtr<ID3D11Device> m_device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
	DeviceCapabilities m_capabilities;
	D3D11RHISwapChain m_swapChain;
	D3D11RHICommandQueue m_graphicsQueue;
	D3D11RHICommandQueue m_computeQueue;
	D3D11RHICommandQueue m_copyQueue;
	ResourcePool<BufferHandle, D3D11BufferResource> m_buffers;
	ResourcePool<TextureHandle, D3D11TextureResource> m_textures;
	ResourcePool<ShaderHandle, D3D11ShaderResource> m_shaders;
	ResourcePool<PipelineStateHandle, D3D11PipelineStateResource> m_pipelines;
	ResourcePool<FenceHandle, std::shared_ptr<D3D11FenceState>> m_fences;
};

} // namespace RHI

#include "D3D11RHIDevice.inl"
