// =======================================================================
// D3D11RHIDevice.h
// =======================================================================
#pragma once

#include <memory>

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
	D3D11RHISwapChain(
		IDXGISwapChain* swapChain,
		const SwapChainDesc& desc
	);

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
	explicit D3D11RHICommandList(D3D11RHIDevice* owner);

	void Attach(D3D11RHIDevice* owner);
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

private:
	D3D11RHIDevice* m_owner = nullptr;
	bool m_recording = false;
	bool m_renderPassActive = false;
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

	BackendType GetBackendType() const override { return BackendType::Direct3D11; }

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

	IRHICommandList& GetImmediateCommandList() override { return m_immediateCommandList; }
	IRHISwapChain* GetSwapChain() override { return &m_swapChain; }
	const IRHISwapChain* GetSwapChain() const override { return &m_swapChain; }
	void WaitIdle() override;

	ID3D11Device* NativeDevice() const { return m_device.Get(); }
	ID3D11DeviceContext* NativeContext() const { return m_context.Get(); }

private:
	friend class D3D11RHICommandList;

	D3D11BufferResource* Find(BufferHandle handle) { return m_buffers.TryGet(handle); }
	D3D11TextureResource* Find(TextureHandle handle) { return m_textures.TryGet(handle); }
	D3D11ShaderResource* Find(ShaderHandle handle) { return m_shaders.TryGet(handle); }
	D3D11PipelineStateResource* Find(PipelineStateHandle handle) { return m_pipelines.TryGet(handle); }
	const D3D11BufferResource* Find(BufferHandle handle) const { return m_buffers.TryGet(handle); }
	const D3D11TextureResource* Find(TextureHandle handle) const { return m_textures.TryGet(handle); }
	const D3D11ShaderResource* Find(ShaderHandle handle) const { return m_shaders.TryGet(handle); }
	const D3D11PipelineStateResource* Find(PipelineStateHandle handle) const { return m_pipelines.TryGet(handle); }

	Microsoft::WRL::ComPtr<ID3D11Device> m_device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
	D3D11RHISwapChain m_swapChain;
	D3D11RHICommandList m_immediateCommandList;
	ResourcePool<BufferHandle, D3D11BufferResource> m_buffers;
	ResourcePool<TextureHandle, D3D11TextureResource> m_textures;
	ResourcePool<ShaderHandle, D3D11ShaderResource> m_shaders;
	ResourcePool<PipelineStateHandle, D3D11PipelineStateResource> m_pipelines;
};

} // namespace RHI

#include "D3D11RHIDevice.inl"
