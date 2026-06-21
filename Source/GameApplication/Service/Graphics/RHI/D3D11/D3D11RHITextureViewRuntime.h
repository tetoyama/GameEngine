#pragma once

#include "D3D11RHIConversions.h"

namespace RHI::D3D11Detail {

inline DXGI_FORMAT ResolveTextureViewFormat(
	const TextureDesc& texture,
	TextureViewType type,
	Format overrideFormat
){
	if(overrideFormat != Format::Unknown){
		return ToDXGIFormat(overrideFormat);
	}
	if(type == TextureViewType::ShaderResource){
		switch(texture.format){
			case Format::D24_UNorm_S8_UInt: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			case Format::D32_Float: return DXGI_FORMAT_R32_FLOAT;
			case Format::D32_Float_S8X24_UInt: return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			default: break;
		}
	}
	return ToDXGIFormat(texture.format);
}

} // namespace RHI::D3D11Detail

namespace RHI {

inline TextureViewHandle D3D11RHIDevice::CreateTextureView(
	const TextureViewDesc& input
){
	D3D11TextureResource* texture = Find(input.texture);
	if(!m_device || !texture || !texture->object) return {};
	if(input.baseArrayLayer != 0 || input.arrayLayerCount != 1) return {};
	if(input.baseMipLevel >= texture->desc.mipLevels || input.mipLevelCount == 0 ||
		input.mipLevelCount > texture->desc.mipLevels - input.baseMipLevel){
		return {};
	}

	TextureBindFlags required = TextureBindFlags::ShaderResource;
	switch(input.type){
		case TextureViewType::UnorderedAccess: required = TextureBindFlags::UnorderedAccess; break;
		case TextureViewType::RenderTarget: required = TextureBindFlags::RenderTarget; break;
		case TextureViewType::DepthStencil: required = TextureBindFlags::DepthStencil; break;
		default: break;
	}
	if(!HasAnyFlag(texture->desc.bindFlags, required)) return {};
	if(input.type != TextureViewType::ShaderResource && input.mipLevelCount != 1) return {};

	const DXGI_FORMAT format = D3D11Detail::ResolveTextureViewFormat(
		texture->desc, input.type, input.format
	);
	if(format == DXGI_FORMAT_UNKNOWN) return {};

	D3D11TextureViewResource view;
	view.desc = input;
	switch(input.type){
		case TextureViewType::ShaderResource: {
			D3D11_SHADER_RESOURCE_VIEW_DESC native{};
			native.Format = format;
			native.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			native.Texture2D.MostDetailedMip = input.baseMipLevel;
			native.Texture2D.MipLevels = input.mipLevelCount;
			if(FAILED(m_device->CreateShaderResourceView(
				texture->object.Get(), &native, view.shaderView.GetAddressOf()
			))) return {};
			break;
		}
		case TextureViewType::UnorderedAccess: {
			D3D11_UNORDERED_ACCESS_VIEW_DESC native{};
			native.Format = format;
			native.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			native.Texture2D.MipSlice = input.baseMipLevel;
			if(FAILED(m_device->CreateUnorderedAccessView(
				texture->object.Get(), &native, view.unorderedAccessView.GetAddressOf()
			))) return {};
			break;
		}
		case TextureViewType::RenderTarget: {
			D3D11_RENDER_TARGET_VIEW_DESC native{};
			native.Format = format;
			native.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			native.Texture2D.MipSlice = input.baseMipLevel;
			if(FAILED(m_device->CreateRenderTargetView(
				texture->object.Get(), &native, view.renderView.GetAddressOf()
			))) return {};
			break;
		}
		case TextureViewType::DepthStencil: {
			D3D11_DEPTH_STENCIL_VIEW_DESC native{};
			native.Format = format;
			native.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			native.Texture2D.MipSlice = input.baseMipLevel;
			if(FAILED(m_device->CreateDepthStencilView(
				texture->object.Get(), &native, view.depthView.GetAddressOf()
			))) return {};
			break;
		}
	}
	++texture->viewCount;
	return m_textureViews.Create(std::move(view));
}

inline bool D3D11RHIDevice::DestroyTextureView(TextureViewHandle handle){
	D3D11TextureViewResource* view = Find(handle);
	if(!view) return false;
	if(D3D11TextureResource* texture = Find(view->desc.texture); texture && texture->viewCount > 0){
		--texture->viewCount;
	}
	return m_textureViews.Destroy(handle);
}

inline const TextureViewDesc* D3D11RHIDevice::GetTextureViewDesc(
	TextureViewHandle handle
) const {
	const auto* view = Find(handle);
	return view ? &view->desc : nullptr;
}

} // namespace RHI
