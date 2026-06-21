#pragma once

#include "D3D11RHIConversions.h"

namespace RHI {

inline TextureHandle D3D11RHIDevice::CreateTexture(
	const TextureDesc& desc,
	std::span<const std::byte> initialData,
	uint32_t initialRowPitch
){
	if(!m_device || desc.width == 0 || desc.height == 0) return {};
	if(desc.depth != 1 || desc.arraySize != 1 || desc.mipLevels == 0) return {};
	if(desc.sampleCount != 1) return {};
	if(!initialData.empty() && (desc.mipLevels != 1 || initialRowPitch == 0)){
		return {};
	}

	DXGI_FORMAT resourceFormat = D3D11Detail::ToDXGIFormat(desc.format);
	DXGI_FORMAT shaderFormat = resourceFormat;
	DXGI_FORMAT depthFormat = resourceFormat;
	const bool depthTexture = HasAnyFlag(
		desc.bindFlags,
		TextureBindFlags::DepthStencil
	);

	if(depthTexture && HasAnyFlag(desc.bindFlags, TextureBindFlags::ShaderResource)){
		switch(desc.format){
			case Format::D24_UNorm_S8_UInt:
				resourceFormat = DXGI_FORMAT_R24G8_TYPELESS;
				shaderFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
				break;
			case Format::D32_Float:
				resourceFormat = DXGI_FORMAT_R32_TYPELESS;
				shaderFormat = DXGI_FORMAT_R32_FLOAT;
				depthFormat = DXGI_FORMAT_D32_FLOAT;
				break;
			case Format::D32_Float_S8X24_UInt:
				resourceFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
				shaderFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				depthFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
				break;
			default:
				break;
		}
	}
	if(resourceFormat == DXGI_FORMAT_UNKNOWN) return {};

	D3D11_TEXTURE2D_DESC nativeDesc{};
	nativeDesc.Width = desc.width;
	nativeDesc.Height = desc.height;
	nativeDesc.MipLevels = desc.mipLevels;
	nativeDesc.ArraySize = 1;
	nativeDesc.Format = resourceFormat;
	nativeDesc.SampleDesc.Count = 1;
	nativeDesc.Usage = D3D11Detail::ToUsage(desc.usage);
	nativeDesc.BindFlags = D3D11Detail::ToTextureBind(desc.bindFlags);
	nativeDesc.CPUAccessFlags = D3D11Detail::ToCpuAccess(desc.cpuAccess);
	if(desc.generateMips){
		nativeDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
	}

	D3D11_SUBRESOURCE_DATA upload{};
	const D3D11_SUBRESOURCE_DATA* uploadPtr = nullptr;
	if(!initialData.empty()){
		upload.pSysMem = initialData.data();
		upload.SysMemPitch = initialRowPitch;
		uploadPtr = &upload;
	}

	D3D11TextureResource resource;
	resource.desc = desc;
	if(FAILED(m_device->CreateTexture2D(
		&nativeDesc,
		uploadPtr,
		resource.object.GetAddressOf()
	))){
		return {};
	}

	if(HasAnyFlag(desc.bindFlags, TextureBindFlags::ShaderResource)){
		D3D11_SHADER_RESOURCE_VIEW_DESC view{};
		view.Format = shaderFormat;
		view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		view.Texture2D.MostDetailedMip = 0;
		view.Texture2D.MipLevels = desc.mipLevels;
		if(FAILED(m_device->CreateShaderResourceView(
			resource.object.Get(),
			&view,
			resource.shaderView.GetAddressOf()
		))){
			return {};
		}
	}

	if(HasAnyFlag(desc.bindFlags, TextureBindFlags::RenderTarget)){
		if(FAILED(m_device->CreateRenderTargetView(
			resource.object.Get(),
			nullptr,
			resource.renderView.GetAddressOf()
		))){
			return {};
		}
	}

	if(depthTexture){
		D3D11_DEPTH_STENCIL_VIEW_DESC view{};
		view.Format = depthFormat;
		view.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		if(FAILED(m_device->CreateDepthStencilView(
			resource.object.Get(),
			&view,
			resource.depthView.GetAddressOf()
		))){
			return {};
		}
	}

	return m_textures.Create(std::move(resource));
}

} // namespace RHI
