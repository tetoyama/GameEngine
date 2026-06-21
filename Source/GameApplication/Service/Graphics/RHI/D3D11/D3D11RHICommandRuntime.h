#pragma once

#include <cstring>
#include <vector>

#include "D3D11RHIConversions.h"

namespace RHI {

inline void D3D11RHICommandList::Begin(){
	m_recording = m_owner && m_owner->m_context;
	m_closed = false;
	m_renderPassActive = false;
}

inline void D3D11RHICommandList::End(){
	if(m_renderPassActive) EndRenderPass();
	m_recording = false;
	m_closed = true;
}

inline bool D3D11RHICommandList::BeginRenderPass(
	const RenderPassDesc& desc
){
	if(!m_recording || !m_owner || !m_owner->m_context ||
		m_queueType != CommandQueueType::Graphics ||
		desc.colorAttachments.size() > 8){
		return false;
	}
	if(m_renderPassActive) EndRenderPass();

	std::vector<ID3D11RenderTargetView*> renderTargets;
	renderTargets.reserve(desc.colorAttachments.size());
	for(const ColorAttachmentDesc& attachment : desc.colorAttachments){
		D3D11TextureResource* texture = m_owner->Find(attachment.texture);
		if(!texture || !texture->renderView) return false;
		renderTargets.push_back(texture->renderView.Get());
		if(attachment.loadOperation == LoadOperation::Clear){
			m_owner->m_context->ClearRenderTargetView(
				texture->renderView.Get(),
				attachment.clearColor.data()
			);
		}
	}

	ID3D11DepthStencilView* depthView = nullptr;
	if(desc.hasDepthAttachment){
		D3D11TextureResource* depth = m_owner->Find(
			desc.depthAttachment.texture
		);
		if(!depth || !depth->depthView) return false;
		depthView = depth->depthView.Get();

		UINT clearFlags = 0;
		if(desc.depthAttachment.depthLoadOperation == LoadOperation::Clear){
			clearFlags |= D3D11_CLEAR_DEPTH;
		}
		if(desc.depthAttachment.stencilLoadOperation == LoadOperation::Clear){
			clearFlags |= D3D11_CLEAR_STENCIL;
		}
		if(clearFlags != 0){
			m_owner->m_context->ClearDepthStencilView(
				depthView,
				clearFlags,
				desc.depthAttachment.clearDepth,
				desc.depthAttachment.clearStencil
			);
		}
	}

	m_owner->m_context->OMSetRenderTargets(
		static_cast<UINT>(renderTargets.size()),
		renderTargets.empty() ? nullptr : renderTargets.data(),
		depthView
	);
	m_renderPassActive = true;
	return true;
}

inline void D3D11RHICommandList::EndRenderPass(){
	if(!m_renderPassActive) return;
	if(!m_owner || !m_owner->m_context){
		m_renderPassActive = false;
		return;
	}
	m_owner->m_context->OMSetRenderTargets(0, nullptr, nullptr);
	m_renderPassActive = false;
}

inline bool D3D11RHICommandList::SetPipelineState(
	PipelineStateHandle pipeline
){
	if(!m_recording || !m_owner || !m_owner->m_context ||
		m_queueType == CommandQueueType::Copy){
		return false;
	}
	D3D11PipelineStateResource* state = m_owner->Find(pipeline);
	if(!state) return false;

	const bool computePipeline = static_cast<bool>(state->desc.computeShader);
	if(m_queueType == CommandQueueType::Compute && !computePipeline){
		return false;
	}

	D3D11ShaderResource* vertex = state->desc.vertexShader
		? m_owner->Find(state->desc.vertexShader)
		: nullptr;
	D3D11ShaderResource* pixel = state->desc.pixelShader
		? m_owner->Find(state->desc.pixelShader)
		: nullptr;
	D3D11ShaderResource* compute = state->desc.computeShader
		? m_owner->Find(state->desc.computeShader)
		: nullptr;

	m_owner->m_context->VSSetShader(
		vertex ? vertex->vertex.Get() : nullptr,
		nullptr,
		0
	);
	m_owner->m_context->PSSetShader(
		pixel ? pixel->pixel.Get() : nullptr,
		nullptr,
		0
	);
	m_owner->m_context->CSSetShader(
		compute ? compute->compute.Get() : nullptr,
		nullptr,
		0
	);
	m_owner->m_context->IASetInputLayout(state->inputLayout.Get());
	m_owner->m_context->IASetPrimitiveTopology(
		D3D11Detail::ToTopology(state->desc.topology)
	);
	if(state->rasterizer){
		m_owner->m_context->RSSetState(state->rasterizer.Get());
	}
	if(state->depthStencil){
		m_owner->m_context->OMSetDepthStencilState(
			state->depthStencil.Get(),
			0
		);
	}
	if(state->blend){
		const float blendFactor[4] = {0, 0, 0, 0};
		m_owner->m_context->OMSetBlendState(
			state->blend.Get(),
			blendFactor,
			0xffffffffu
		);
	}
	return true;
}

inline void D3D11RHICommandList::SetViewport(const Viewport& viewport){
	if(!m_recording || !m_owner || !m_owner->m_context ||
		m_queueType != CommandQueueType::Graphics){
		return;
	}
	D3D11_VIEWPORT native{};
	native.TopLeftX = viewport.x;
	native.TopLeftY = viewport.y;
	native.Width = viewport.width;
	native.Height = viewport.height;
	native.MinDepth = viewport.minDepth;
	native.MaxDepth = viewport.maxDepth;
	m_owner->m_context->RSSetViewports(1, &native);
}

inline bool D3D11RHICommandList::SetVertexBuffer(
	uint32_t slot,
	BufferHandle buffer,
	uint32_t stride,
	uint32_t offset
){
	if(!m_recording || !m_owner || !m_owner->m_context ||
		m_queueType != CommandQueueType::Graphics){
		return false;
	}
	D3D11BufferResource* resource = m_owner->Find(buffer);
	if(!resource || !resource->object) return false;
	ID3D11Buffer* native = resource->object.Get();
	m_owner->m_context->IASetVertexBuffers(
		slot,
		1,
		&native,
		&stride,
		&offset
	);
	return true;
}

inline bool D3D11RHICommandList::SetIndexBuffer(
	BufferHandle buffer,
	IndexFormat format,
	uint32_t offset
){
	if(!m_recording || !m_owner || !m_owner->m_context ||
		m_queueType != CommandQueueType::Graphics){
		return false;
	}
	D3D11BufferResource* resource = m_owner->Find(buffer);
	if(!resource || !resource->object) return false;
	m_owner->m_context->IASetIndexBuffer(
		resource->object.Get(),
		format == IndexFormat::UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
		offset
	);
	return true;
}

inline bool D3D11RHICommandList::SetConstantBuffer(
	ShaderStage stage,
	uint32_t slot,
	BufferHandle buffer
){
	if(!m_recording || !m_owner || !m_owner->m_context ||
		m_queueType == CommandQueueType::Copy ||
		(m_queueType == CommandQueueType::Compute &&
			stage != ShaderStage::Compute)){
		return false;
	}
	D3D11BufferResource* resource = m_owner->Find(buffer);
	if(!resource || !resource->object) return false;
	ID3D11Buffer* native = resource->object.Get();
	switch(stage){
		case ShaderStage::Vertex:
			m_owner->m_context->VSSetConstantBuffers(slot, 1, &native);
			break;
		case ShaderStage::Pixel:
			m_owner->m_context->PSSetConstantBuffers(slot, 1, &native);
			break;
		case ShaderStage::Compute:
			m_owner->m_context->CSSetConstantBuffers(slot, 1, &native);
			break;
	}
	return true;
}

inline bool D3D11RHICommandList::SetTexture(
	ShaderStage stage,
	uint32_t slot,
	TextureHandle texture
){
	if(!m_recording || !m_owner || !m_owner->m_context ||
		m_queueType == CommandQueueType::Copy ||
		(m_queueType == CommandQueueType::Compute &&
			stage != ShaderStage::Compute)){
		return false;
	}
	D3D11TextureResource* resource = m_owner->Find(texture);
	if(!resource || !resource->shaderView) return false;
	ID3D11ShaderResourceView* native = resource->shaderView.Get();
	switch(stage){
		case ShaderStage::Vertex:
			m_owner->m_context->VSSetShaderResources(slot, 1, &native);
			break;
		case ShaderStage::Pixel:
			m_owner->m_context->PSSetShaderResources(slot, 1, &native);
			break;
		case ShaderStage::Compute:
			m_owner->m_context->CSSetShaderResources(slot, 1, &native);
			break;
	}
	return true;
}

inline bool D3D11RHICommandList::UpdateBuffer(
	BufferHandle buffer,
	std::span<const std::byte> data,
	uint32_t destinationOffset
){
	if(!m_recording || !m_owner || !m_owner->m_context) return false;
	D3D11BufferResource* resource = m_owner->Find(buffer);
	if(!resource || !resource->object) return false;
	if(data.empty()) return true;
	if(destinationOffset + data.size() > resource->desc.byteSize) return false;

	if(resource->desc.usage == ResourceUsage::Dynamic){
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if(FAILED(m_owner->m_context->Map(
			resource->object.Get(),
			0,
			D3D11_MAP_WRITE_DISCARD,
			0,
			&mapped
		))){
			return false;
		}
		std::memcpy(
			static_cast<std::byte*>(mapped.pData) + destinationOffset,
			data.data(),
			data.size()
		);
		m_owner->m_context->Unmap(resource->object.Get(), 0);
		return true;
	}

	D3D11_BOX box{};
	box.left = destinationOffset;
	box.right = destinationOffset + static_cast<UINT>(data.size());
	box.top = 0;
	box.bottom = 1;
	box.front = 0;
	box.back = 1;
	m_owner->m_context->UpdateSubresource(
		resource->object.Get(),
		0,
		&box,
		data.data(),
		0,
		0
	);
	return true;
}

inline void D3D11RHICommandList::Draw(
	uint32_t vertexCount,
	uint32_t firstVertex
){
	if(m_recording && m_owner && m_owner->m_context &&
		m_queueType == CommandQueueType::Graphics){
		m_owner->m_context->Draw(vertexCount, firstVertex);
	}
}

inline void D3D11RHICommandList::DrawIndexed(
	uint32_t indexCount,
	uint32_t firstIndex,
	int32_t vertexOffset
){
	if(m_recording && m_owner && m_owner->m_context &&
		m_queueType == CommandQueueType::Graphics){
		m_owner->m_context->DrawIndexed(indexCount, firstIndex, vertexOffset);
	}
}

inline void D3D11RHICommandList::Dispatch(
	uint32_t groupCountX,
	uint32_t groupCountY,
	uint32_t groupCountZ
){
	if(m_recording && m_owner && m_owner->m_context &&
		m_queueType != CommandQueueType::Copy){
		m_owner->m_context->Dispatch(groupCountX, groupCountY, groupCountZ);
	}
}

} // namespace RHI
