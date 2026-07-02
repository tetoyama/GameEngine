#pragma once

#include <vector>

#include "D3D11RHIConversions.h"

namespace RHI {

inline PipelineStateHandle D3D11RHIDevice::CreatePipelineState(
	const PipelineStateDesc& desc
){
	if(!m_device) return {};
	if(desc.renderTargets.colorAttachmentCount > 8) return {};
	if(desc.renderTargets.sampleCount == 0) return {};

	const D3D11ShaderResource* vertex = desc.vertexShader
		? Find(desc.vertexShader)
		: nullptr;
	const D3D11ShaderResource* pixel = desc.pixelShader
		? Find(desc.pixelShader)
		: nullptr;
	const D3D11ShaderResource* compute = desc.computeShader
		? Find(desc.computeShader)
		: nullptr;

	if(desc.vertexShader && (!vertex || !vertex->vertex)) return {};
	if(desc.pixelShader && (!pixel || !pixel->pixel)) return {};
	if(desc.computeShader && (!compute || !compute->compute)) return {};
	if(desc.computeShader && (desc.vertexShader || desc.pixelShader)) return {};

	for(uint8_t index = 0;
		index < desc.renderTargets.colorAttachmentCount;
		++index){
		if(D3D11Detail::ToDXGIFormat(
			desc.renderTargets.colorFormats[index]
		) == DXGI_FORMAT_UNKNOWN){
			return {};
		}
	}
	if(desc.renderTargets.depthStencilFormat != Format::Unknown &&
		D3D11Detail::ToDXGIFormat(
			desc.renderTargets.depthStencilFormat
		) == DXGI_FORMAT_UNKNOWN){
		return {};
	}

	D3D11PipelineStateResource resource;
	resource.desc = desc;

	if(!desc.inputLayout.empty()){
		if(!vertex || vertex->byteCode.empty()) return {};

		std::vector<D3D11_INPUT_ELEMENT_DESC> nativeElements;
		nativeElements.reserve(desc.inputLayout.size());
		for(const InputElementDesc& source : desc.inputLayout){
			const DXGI_FORMAT format = D3D11Detail::ToDXGIFormat(
				source.format
			);
			if(format == DXGI_FORMAT_UNKNOWN || source.semanticName.empty()){
				return {};
			}

			D3D11_INPUT_ELEMENT_DESC element{};
			element.SemanticName = source.semanticName.c_str();
			element.SemanticIndex = source.semanticIndex;
			element.Format = format;
			element.InputSlot = source.inputSlot;
			element.AlignedByteOffset = source.alignedByteOffset;
			element.InputSlotClass = source.perInstance
				? D3D11_INPUT_PER_INSTANCE_DATA
				: D3D11_INPUT_PER_VERTEX_DATA;
			element.InstanceDataStepRate = source.perInstance
				? source.instanceStepRate
				: 0;
			nativeElements.push_back(element);
		}

		if(FAILED(m_device->CreateInputLayout(
			nativeElements.data(),
			static_cast<UINT>(nativeElements.size()),
			vertex->byteCode.data(),
			vertex->byteCode.size(),
			resource.inputLayout.GetAddressOf()
		))){
			return {};
		}
	}

	D3D11_RASTERIZER_DESC rasterizer{};
	rasterizer.FillMode = D3D11Detail::ToFillMode(
		desc.rasterizer.fillMode
	);
	rasterizer.CullMode = D3D11Detail::ToCullMode(
		desc.rasterizer.cullMode
	);
	rasterizer.FrontCounterClockwise =
		desc.rasterizer.frontCounterClockwise;
	rasterizer.DepthClipEnable = desc.rasterizer.depthClipEnable;
	rasterizer.ScissorEnable = desc.rasterizer.scissorEnable;
	rasterizer.MultisampleEnable = desc.rasterizer.multisampleEnable;
	if(FAILED(m_device->CreateRasterizerState(
		&rasterizer,
		resource.rasterizer.GetAddressOf()
	))){
		return {};
	}

	D3D11_DEPTH_STENCIL_DESC depthStencil{};
	depthStencil.DepthEnable = desc.depthStencil.depthEnable;
	depthStencil.DepthWriteMask = desc.depthStencil.depthWriteEnable
		? D3D11_DEPTH_WRITE_MASK_ALL
		: D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencil.DepthFunc = D3D11Detail::ToComparison(
		desc.depthStencil.depthFunction
	);
	depthStencil.StencilEnable = FALSE;
	if(FAILED(m_device->CreateDepthStencilState(
		&depthStencil,
		resource.depthStencil.GetAddressOf()
	))){
		return {};
	}

	D3D11_BLEND_DESC blend{};
	blend.AlphaToCoverageEnable = desc.blend.alphaToCoverageEnable;
	blend.IndependentBlendEnable = desc.blend.independentBlendEnable;
	for(size_t index = 0; index < desc.blend.targets.size(); ++index){
		const BlendTargetDesc& source = desc.blend.targets[index];
		D3D11_RENDER_TARGET_BLEND_DESC& target = blend.RenderTarget[index];
		target.BlendEnable = source.blendEnable;
		target.SrcBlend = D3D11Detail::ToBlend(source.sourceColor);
		target.DestBlend = D3D11Detail::ToBlend(
			source.destinationColor
		);
		target.BlendOp = D3D11Detail::ToBlendOperation(
			source.colorOperation
		);
		target.SrcBlendAlpha = D3D11Detail::ToBlend(
			source.sourceAlpha
		);
		target.DestBlendAlpha = D3D11Detail::ToBlend(
			source.destinationAlpha
		);
		target.BlendOpAlpha = D3D11Detail::ToBlendOperation(
			source.alphaOperation
		);
		target.RenderTargetWriteMask = source.writeMask;
	}
	if(FAILED(m_device->CreateBlendState(
		&blend,
		resource.blend.GetAddressOf()
	))){
		return {};
	}

	return m_pipelines.Create(std::move(resource));
}

} // namespace RHI
