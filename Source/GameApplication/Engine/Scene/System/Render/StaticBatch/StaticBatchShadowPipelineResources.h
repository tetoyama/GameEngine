#pragma once

#include <cstddef>
#include <span>

#include "Service/Graphics/RHI/RHIInterfaces.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchInstanceInputLayout.h"

namespace StaticBatchShadowPipelineContract {

inline RHI::PipelineStateDesc BuildPipelineStateDesc(
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
	desc.rasterizer.depthClipEnable = false;
	desc.depthStencil.depthEnable = true;
	desc.depthStencil.depthWriteEnable = true;
	desc.depthStencil.depthFunction = RHI::ComparisonFunc::LessEqual;
	desc.renderTargets.colorAttachmentCount = 0;
	desc.renderTargets.depthStencilFormat = RHI::Format::D32_Float;
	desc.renderTargets.sampleCount = 1;
	desc.debugName = "Static Batch Shadow Pipeline";
	return desc;
}

} // namespace StaticBatchShadowPipelineContract

class StaticBatchShadowPipelineResources {
public:
	StaticBatchShadowPipelineResources() = default;
	StaticBatchShadowPipelineResources(
		const StaticBatchShadowPipelineResources&
	) = delete;
	StaticBatchShadowPipelineResources& operator=(
		const StaticBatchShadowPipelineResources&
	) = delete;
	StaticBatchShadowPipelineResources(
		StaticBatchShadowPipelineResources&&
	) = delete;
	StaticBatchShadowPipelineResources& operator=(
		StaticBatchShadowPipelineResources&&
	) = delete;

	bool Create(
		RHI::IRHIDevice& device,
		RHI::ShaderHandle sharedVertexShader,
		std::span<const std::byte> pixelShaderByteCode
	){
		if(IsAllocated() || !sharedVertexShader || pixelShaderByteCode.empty()){
			return false;
		}

		RHI::ShaderDesc shaderDesc;
		shaderDesc.stage = RHI::ShaderStage::Pixel;
		shaderDesc.entryPoint = "main";
		shaderDesc.debugName = "Static Batch Shadow PS";
		m_pixelShader = device.CreateShader(shaderDesc, pixelShaderByteCode);
		if(!m_pixelShader) return false;

		m_pipelineState = device.CreatePipelineState(
			StaticBatchShadowPipelineContract::BuildPipelineStateDesc(
				sharedVertexShader,
				m_pixelShader
			)
		);
		if(!m_pipelineState){
			Release(device);
			return false;
		}
		return true;
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
		if(!m_pipelineState && m_pixelShader){
			if(device.DestroyShader(m_pixelShader)){
				m_pixelShader = {};
			}else{
				released = false;
			}
		}
		return released && !IsAllocated();
	}

	bool IsReady() const noexcept {
		return static_cast<bool>(m_pixelShader) &&
			static_cast<bool>(m_pipelineState);
	}

	bool IsAllocated() const noexcept {
		return static_cast<bool>(m_pixelShader) ||
			static_cast<bool>(m_pipelineState);
	}

	RHI::ShaderHandle PixelShader() const noexcept { return m_pixelShader; }
	RHI::PipelineStateHandle PipelineState() const noexcept {
		return m_pipelineState;
	}

private:
	RHI::ShaderHandle m_pixelShader;
	RHI::PipelineStateHandle m_pipelineState;
};
