#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include "RHICapabilities.h"
#include "RHIDescriptors.h"
#include "RHIExecution.h"

namespace RHI {

class IRHICommandList {
public:
	virtual ~IRHICommandList() = default;
	virtual CommandQueueType GetQueueType() const noexcept = 0;
	virtual void Begin() = 0;
	virtual void End() = 0;
	virtual bool ResourceBarrier(std::span<const ResourceBarrierDesc> barriers) = 0;
	virtual bool BeginRenderPass(const RenderPassDesc& desc) = 0;
	virtual void EndRenderPass() = 0;
	virtual bool SetPipelineState(PipelineStateHandle pipeline) = 0;
	virtual void SetViewport(const Viewport& viewport) = 0;
	virtual bool SetVertexBuffer(uint32_t slot, BufferHandle buffer, uint32_t stride, uint32_t offset = 0) = 0;
	virtual bool SetIndexBuffer(BufferHandle buffer, IndexFormat format, uint32_t offset = 0) = 0;
	virtual bool SetConstantBuffer(ShaderStage stage, uint32_t slot, BufferHandle buffer) = 0;
	virtual bool SetBufferView(ShaderStage stage, uint32_t slot, BufferViewHandle view) = 0;
	virtual bool SetTextureView(ShaderStage stage, uint32_t slot, TextureViewHandle view) = 0;
	virtual bool SetSampler(ShaderStage stage, uint32_t slot, SamplerHandle sampler) = 0;
	virtual bool UpdateBuffer(BufferHandle buffer, std::span<const std::byte> data, uint32_t destinationOffset = 0) = 0;
	virtual void Draw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;
	virtual void DrawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0) = 0;
	virtual bool DrawIndexedInstanced(
		uint32_t indexCountPerInstance,
		uint32_t instanceCount,
		uint32_t firstIndex = 0,
		int32_t vertexOffset = 0,
		uint32_t firstInstance = 0
	){
		(void)indexCountPerInstance;
		(void)instanceCount;
		(void)firstIndex;
		(void)vertexOffset;
		(void)firstInstance;
		return false;
	}
	virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
};

class IRHISwapChain {
public:
	virtual ~IRHISwapChain() = default;
	virtual bool Resize(uint32_t width, uint32_t height) = 0;
	virtual bool Present(bool verticalSync) = 0;
	virtual const SwapChainDesc& GetDesc() const = 0;
	virtual uint32_t GetImageCount() const noexcept = 0;
	virtual uint32_t GetCurrentImageIndex() const noexcept = 0;
	virtual TextureHandle GetImage(uint32_t imageIndex) const noexcept = 0;
	virtual CommandQueueType GetPresentQueueType() const noexcept = 0;
	TextureHandle GetCurrentImage() const noexcept { return GetImage(GetCurrentImageIndex()); }
};

class IRHIDevice {
public:
	virtual ~IRHIDevice() = default;
	virtual BackendType GetBackendType() const noexcept = 0;
	virtual const DeviceCapabilities& GetCapabilities() const noexcept = 0;
	virtual BufferHandle CreateBuffer(const BufferDesc& desc, std::span<const std::byte> initialData = {}) = 0;
	virtual TextureHandle CreateTexture(const TextureDesc& desc, std::span<const std::byte> initialData = {}, uint32_t initialRowPitch = 0) = 0;
	virtual BufferViewHandle CreateBufferView(const BufferViewDesc& desc) = 0;
	virtual TextureViewHandle CreateTextureView(const TextureViewDesc& desc) = 0;
	virtual SamplerHandle CreateSampler(const SamplerDesc& desc) = 0;
	virtual ShaderHandle CreateShader(const ShaderDesc& desc, std::span<const std::byte> byteCode) = 0;
	virtual PipelineStateHandle CreatePipelineState(const PipelineStateDesc& desc) = 0;
	virtual bool DestroyBuffer(BufferHandle handle) = 0;
	virtual bool DestroyTexture(TextureHandle handle) = 0;
	virtual bool DestroyBufferView(BufferViewHandle handle) = 0;
	virtual bool DestroyTextureView(TextureViewHandle handle) = 0;
	virtual bool DestroySampler(SamplerHandle handle) = 0;
	virtual bool DestroyShader(ShaderHandle handle) = 0;
	virtual bool DestroyPipelineState(PipelineStateHandle handle) = 0;
	virtual const BufferDesc* GetBufferDesc(BufferHandle handle) const = 0;
	virtual const TextureDesc* GetTextureDesc(TextureHandle handle) const = 0;
	virtual const BufferViewDesc* GetBufferViewDesc(BufferViewHandle handle) const = 0;
	virtual const TextureViewDesc* GetTextureViewDesc(TextureViewHandle handle) const = 0;
	virtual const SamplerDesc* GetSamplerDesc(SamplerHandle handle) const = 0;
	virtual const ShaderDesc* GetShaderDesc(ShaderHandle handle) const = 0;
	virtual const PipelineStateDesc* GetPipelineStateDesc(PipelineStateHandle handle) const = 0;
	virtual IRHICommandQueue* GetQueue(CommandQueueType type) = 0;
	virtual const IRHICommandQueue* GetQueue(CommandQueueType type) const = 0;
	virtual std::unique_ptr<IRHICommandList> CreateCommandList(const CommandListCreateDesc& desc) = 0;
	virtual std::unique_ptr<IRHIFence> CreateFence(uint64_t initialValue = 0) = 0;
	virtual bool DestroyFence(FenceHandle handle) = 0;
	virtual IRHISwapChain* GetSwapChain() = 0;
	virtual const IRHISwapChain* GetSwapChain() const = 0;
	virtual void WaitIdle() = 0;
	bool PresentSwapChain(bool verticalSync){
		IRHISwapChain* swapChain = GetSwapChain();
		if(!swapChain) return false;
		IRHICommandQueue* queue = GetQueue(swapChain->GetPresentQueueType());
		return queue && queue->Present(*swapChain, verticalSync);
	}
	bool ResizeSwapChain(uint32_t width, uint32_t height){
		IRHISwapChain* swapChain = GetSwapChain();
		if(!swapChain || width == 0 || height == 0) return false;
		WaitIdle();
		return swapChain->Resize(width, height);
	}
};

} // namespace RHI
