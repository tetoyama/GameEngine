// =======================================================================
//
// RHIInterfaces.h
//
// GPU Backendが実装する最小RHI契約。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "RHIDescriptors.h"

namespace RHI {

class IRHICommandList {
public:
	virtual ~IRHICommandList() = default;

	virtual void Begin() = 0;
	virtual void End() = 0;

	virtual bool BeginRenderPass(const RenderPassDesc& desc) = 0;
	virtual void EndRenderPass() = 0;

	virtual bool SetPipelineState(PipelineStateHandle pipeline) = 0;
	virtual void SetViewport(const Viewport& viewport) = 0;
	virtual bool SetVertexBuffer(
		uint32_t slot,
		BufferHandle buffer,
		uint32_t stride,
		uint32_t offset = 0
	) = 0;
	virtual bool SetIndexBuffer(
		BufferHandle buffer,
		IndexFormat format,
		uint32_t offset = 0
	) = 0;
	virtual bool SetConstantBuffer(
		ShaderStage stage,
		uint32_t slot,
		BufferHandle buffer
	) = 0;
	virtual bool SetTexture(
		ShaderStage stage,
		uint32_t slot,
		TextureHandle texture
	) = 0;
	virtual bool UpdateBuffer(
		BufferHandle buffer,
		std::span<const std::byte> data,
		uint32_t destinationOffset = 0
	) = 0;

	virtual void Draw(
		uint32_t vertexCount,
		uint32_t firstVertex = 0
	) = 0;
	virtual void DrawIndexed(
		uint32_t indexCount,
		uint32_t firstIndex = 0,
		int32_t vertexOffset = 0
	) = 0;
	virtual void Dispatch(
		uint32_t groupCountX,
		uint32_t groupCountY,
		uint32_t groupCountZ
	) = 0;
};

class IRHISwapChain {
public:
	virtual ~IRHISwapChain() = default;

	virtual bool Resize(uint32_t width, uint32_t height) = 0;
	virtual bool Present(bool verticalSync) = 0;
	virtual const SwapChainDesc& GetDesc() const = 0;
};

class IRHIDevice {
public:
	virtual ~IRHIDevice() = default;

	virtual BackendType GetBackendType() const = 0;

	virtual BufferHandle CreateBuffer(
		const BufferDesc& desc,
		std::span<const std::byte> initialData = {}
	) = 0;
	virtual TextureHandle CreateTexture(
		const TextureDesc& desc,
		std::span<const std::byte> initialData = {},
		uint32_t initialRowPitch = 0
	) = 0;
	virtual ShaderHandle CreateShader(
		const ShaderDesc& desc,
		std::span<const std::byte> byteCode
	) = 0;
	virtual PipelineStateHandle CreatePipelineState(
		const PipelineStateDesc& desc
	) = 0;

	virtual bool DestroyBuffer(BufferHandle handle) = 0;
	virtual bool DestroyTexture(TextureHandle handle) = 0;
	virtual bool DestroyShader(ShaderHandle handle) = 0;
	virtual bool DestroyPipelineState(PipelineStateHandle handle) = 0;

	virtual const BufferDesc* GetBufferDesc(BufferHandle handle) const = 0;
	virtual const TextureDesc* GetTextureDesc(TextureHandle handle) const = 0;
	virtual const ShaderDesc* GetShaderDesc(ShaderHandle handle) const = 0;
	virtual const PipelineStateDesc* GetPipelineStateDesc(
		PipelineStateHandle handle
	) const = 0;

	virtual IRHICommandList& GetImmediateCommandList() = 0;
	virtual IRHISwapChain* GetSwapChain() = 0;
	virtual const IRHISwapChain* GetSwapChain() const = 0;

	virtual void WaitIdle() = 0;
};

} // namespace RHI
