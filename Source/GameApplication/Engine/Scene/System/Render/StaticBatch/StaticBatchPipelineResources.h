#pragma once

#include <cstddef>
#include <span>

#include "Service/Graphics/RHI/RHIInterfaces.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchInstanceInputLayout.h"

namespace StaticBatchPipelineContract {

inline constexpr std::uint8_t GBufferColorAttachmentCount = 6;

inline RHI::RenderTargetLayoutDesc BuildGBufferRenderTargetLayout(){
	RHI::RenderTargetLayoutDesc layout;
	layout.colorAttachmentCount = GBufferColorAttachmentCount;
	layout.colorFormats[0] = RHI::Format::RGBA16_Float;
	layout.colorFormats[1] = RHI::Format::RGBA16_Float;
	layout.colorFormats[2] = RHI::Format::RGBA16_Float;
	layout.colorFormats[3] = RHI::Format::RGBA16_Float;
	layout.colorFormats[4] = RHI::Format::RGBA16_Float;
	layout.colorFormats[5] = RHI::Format::RGBA32_UInt;
	layout.depthStencilFormat = RHI::Format::D32_Float;
	layout.sampleCount = 1;
	return layout;
}

inline RHI::PipelineStateDesc BuildGBufferPipelineStateDesc(
	RHI::ShaderHandle vertexShader,
	RHI::ShaderHandle pixelShader
){
	RHI::PipelineStateDesc desc;
	desc.vertexShader = vertexShader;
	desc.pixelShader = pixelShader;
	desc.inputLayout = StaticBatchInstanceInputLayout::BuildFull();
	desc.topology = RHI::PrimitiveTopology::TriangleList;
	desc.rasterizer.fillMode = RHI::FillMode::Solid;
	desc.rasterizer.cullMode = RHI::CullMode::Back;
	desc.rasterizer.frontCounterClockwise = false;
	desc.rasterizer.depthClipEnable = true;
	desc.depthStencil.depthEnable = true;
	desc.depthStencil.depthWriteEnable = true;
	desc.depthStencil.depthFunction = RHI::ComparisonFunc::LessEqual;
	desc.renderTargets = BuildGBufferRenderTargetLayout();
	desc.debugName = "Static Batch GBuffer Pipeline";
	return desc;
}

} // namespace StaticBatchPipelineContract

class StaticBatchPipelineResources {
public:
	StaticBatchPipelineResources() = default;
	StaticBatchPipelineResources(const StaticBatchPipelineResources&) = delete;
	StaticBatchPipelineResources& operator=(const StaticBatchPipelineResources&) = delete;
	StaticBatchPipelineResources(StaticBatchPipelineResources&&) = delete;
	StaticBatchPipelineResources& operator=(StaticBatchPipelineResources&&) = delete;

	bool Create(
		RHI::IRHIDevice& device,
		std::span<const std::byte> vertexShaderByteCode,
		std::span<const std::byte> pixelShaderByteCode
	){
		if(IsAllocated() || vertexShaderByteCode.empty() ||
			pixelShaderByteCode.empty()){
			return false;
		}

		RHI::ShaderDesc vertexShaderDesc;
		vertexShaderDesc.stage = RHI::ShaderStage::Vertex;
		vertexShaderDesc.entryPoint = "main";
		vertexShaderDesc.debugName = "Static Batch GBuffer VS";
		m_vertexShader = device.CreateShader(
			vertexShaderDesc,
			vertexShaderByteCode
		);
		if(!m_vertexShader) return false;

		RHI::ShaderDesc pixelShaderDesc;
		pixelShaderDesc.stage = RHI::ShaderStage::Pixel;
		pixelShaderDesc.entryPoint = "main";
		pixelShaderDesc.debugName = "Static Batch GBuffer PS";
		m_pixelShader = device.CreateShader(
			pixelShaderDesc,
			pixelShaderByteCode
		);
		if(!m_pixelShader){
			Release(device);
			return false;
		}

		const RHI::PipelineStateDesc pipelineDesc =
			StaticBatchPipelineContract::BuildGBufferPipelineStateDesc(
				m_vertexShader,
				m_pixelShader
			);
		m_pipelineState = device.CreatePipelineState(pipelineDesc);
		if(!m_pipelineState){
			Release(device);
			return false;
		}

		return true;
	}

	bool Bind(RHI::IRHICommandList& commandList) const {
		return IsReady() && commandList.SetPipelineState(m_pipelineState);
	}

	bool Release(RHI::IRHIDevice& device) noexcept {
		bool released = true;

		if(m_pipelineState){
			if(device.DestroyPipelineState(m_pipelineState)){
				m_pipelineState = {};
			}else{
				released = false;
			}
		}

		// Pipeline descriptors retain Shader handles. Keep the shaders alive when
		// pipeline destruction fails so a later Release can safely retry.
		if(!m_pipelineState){
			if(m_pixelShader){
				if(device.DestroyShader(m_pixelShader)){
					m_pixelShader = {};
				}else{
					released = false;
				}
			}
			if(m_vertexShader){
				if(device.DestroyShader(m_vertexShader)){
					m_vertexShader = {};
				}else{
					released = false;
				}
			}
		}

		return released && !IsAllocated();
	}

	bool IsReady() const noexcept {
		return static_cast<bool>(m_vertexShader) &&
			static_cast<bool>(m_pixelShader) &&
			static_cast<bool>(m_pipelineState);
	}

	bool IsAllocated() const noexcept {
		return static_cast<bool>(m_vertexShader) ||
			static_cast<bool>(m_pixelShader) ||
			static_cast<bool>(m_pipelineState);
	}

	RHI::ShaderHandle VertexShader() const noexcept { return m_vertexShader; }
	RHI::ShaderHandle PixelShader() const noexcept { return m_pixelShader; }
	RHI::PipelineStateHandle PipelineState() const noexcept {
		return m_pipelineState;
	}

private:
	RHI::ShaderHandle m_vertexShader;
	RHI::ShaderHandle m_pixelShader;
	RHI::PipelineStateHandle m_pipelineState;
};
