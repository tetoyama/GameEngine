#pragma once

#include "D3D11RHIConversions.h"

namespace RHI::D3D11Detail {

inline uint32_t BufferViewFormatByteSize(Format format){
	switch(format){
		case Format::R8_UNorm: return 1;
		case Format::RG8_UNorm:
		case Format::R16_UInt:
		case Format::R16_Float: return 2;
		case Format::RGBA8_UNorm:
		case Format::BGRA8_UNorm:
		case Format::R32_UInt:
		case Format::R32_Float:
		case Format::RG16_Float: return 4;
		case Format::RG32_Float:
		case Format::RGBA16_Float: return 8;
		case Format::RGBA32_Float:
		case Format::RGBA32_UInt: return 16;
		default: return 0;
	}
}

} // namespace RHI::D3D11Detail

namespace RHI {

inline BufferViewHandle D3D11RHIDevice::CreateBufferView(
	const BufferViewDesc& input
){
	D3D11BufferResource* buffer = Find(input.buffer);
	if(!m_device || !buffer || !buffer->object) return {};

	const BufferBindFlags required = input.type == BufferViewType::ShaderResource
		? BufferBindFlags::ShaderResource
		: BufferBindFlags::UnorderedAccess;
	if(!HasAnyFlag(buffer->desc.bindFlags, required)) return {};

	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	uint32_t elementSize = 0;
	UINT rawFlags = 0;
	if(buffer->desc.structured){
		if(buffer->desc.stride == 0 || input.format != Format::Unknown) return {};
		elementSize = buffer->desc.stride;
	}
	else if(buffer->desc.raw){
		if(input.format != Format::Unknown) return {};
		format = DXGI_FORMAT_R32_TYPELESS;
		elementSize = 4;
		rawFlags = input.type == BufferViewType::ShaderResource
			? D3D11_BUFFEREX_SRV_FLAG_RAW
			: D3D11_BUFFER_UAV_FLAG_RAW;
	}
	else{
		elementSize = D3D11Detail::BufferViewFormatByteSize(input.format);
		format = D3D11Detail::ToDXGIFormat(input.format);
		if(elementSize == 0 || format == DXGI_FORMAT_UNKNOWN) return {};
	}

	const uint32_t totalElements = buffer->desc.byteSize / elementSize;
	if(input.firstElement >= totalElements) return {};
	const uint32_t count = input.elementCount == 0
		? totalElements - input.firstElement
		: input.elementCount;
	if(count == 0 || count > totalElements - input.firstElement) return {};

	D3D11BufferViewResource view;
	view.desc = input;
	view.desc.elementCount = count;
	if(input.type == BufferViewType::ShaderResource){
		D3D11_SHADER_RESOURCE_VIEW_DESC native{};
		native.Format = format;
		native.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		native.BufferEx.FirstElement = input.firstElement;
		native.BufferEx.NumElements = count;
		native.BufferEx.Flags = rawFlags;
		if(FAILED(m_device->CreateShaderResourceView(
			buffer->object.Get(), &native, view.shaderView.GetAddressOf()
		))) return {};
	}
	else{
		D3D11_UNORDERED_ACCESS_VIEW_DESC native{};
		native.Format = format;
		native.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		native.Buffer.FirstElement = input.firstElement;
		native.Buffer.NumElements = count;
		native.Buffer.Flags = rawFlags;
		if(FAILED(m_device->CreateUnorderedAccessView(
			buffer->object.Get(), &native, view.unorderedAccessView.GetAddressOf()
		))) return {};
	}
	++buffer->viewCount;
	return m_bufferViews.Create(std::move(view));
}

inline bool D3D11RHIDevice::DestroyBufferView(BufferViewHandle handle){
	D3D11BufferViewResource* view = Find(handle);
	if(!view) return false;
	if(D3D11BufferResource* buffer = Find(view->desc.buffer); buffer && buffer->viewCount > 0){
		--buffer->viewCount;
	}
	return m_bufferViews.Destroy(handle);
}

inline const BufferViewDesc* D3D11RHIDevice::GetBufferViewDesc(
	BufferViewHandle handle
) const {
	const auto* view = Find(handle);
	return view ? &view->desc : nullptr;
}

} // namespace RHI
