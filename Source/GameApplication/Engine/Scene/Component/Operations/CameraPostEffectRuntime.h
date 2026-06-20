// =======================================================================
//
// CameraPostEffectRuntime.h
//
// CameraPostEffect定義後にincludeする実装ヘッダー。
// GPUリソース所有権そのものはRenderWorld移行時にPostEffectPass側へ移す。
//
// =======================================================================
#pragma once

#include <algorithm>

inline void CameraPostEffect::CreateTexture(
	ID3D11Device* device,
	const Vector2& screenSize
){
	if(!device || tex){
		return;
	}

	resolution.x = (std::max)(screenSize.x, 1.0f);
	resolution.y = (std::max)(screenSize.y, 1.0f);
	const UINT actualMipLevels = static_cast<UINT>((std::max)(1, mipLevels));

	D3D11_TEXTURE2D_DESC textureDescription{};
	textureDescription.Width = static_cast<UINT>(resolution.x);
	textureDescription.Height = static_cast<UINT>(resolution.y);
	textureDescription.MipLevels = actualMipLevels;
	textureDescription.ArraySize = 1;
	textureDescription.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	textureDescription.SampleDesc.Count = 1;
	textureDescription.Usage = D3D11_USAGE_DEFAULT;
	textureDescription.BindFlags =
		D3D11_BIND_RENDER_TARGET |
		D3D11_BIND_SHADER_RESOURCE;

	if(actualMipLevels > 1){
		textureDescription.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	}

	HRESULT result = device->CreateTexture2D(
		&textureDescription,
		nullptr,
		&tex
	);
	if(FAILED(result)){
		OutputDebugStringA("Failed to create post effect texture\n");
		return;
	}

	D3D11_RENDER_TARGET_VIEW_DESC renderTargetDescription{};
	renderTargetDescription.Format = textureDescription.Format;
	renderTargetDescription.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	renderTargetDescription.Texture2D.MipSlice = 0;

	result = device->CreateRenderTargetView(
		tex.Get(),
		&renderTargetDescription,
		&rtv
	);
	if(FAILED(result)){
		tex.Reset();
		OutputDebugStringA("Failed to create post effect RTV\n");
		return;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC resourceDescription{};
	resourceDescription.Format = textureDescription.Format;
	resourceDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	resourceDescription.Texture2D.MostDetailedMip = 0;
	resourceDescription.Texture2D.MipLevels = actualMipLevels;

	result = device->CreateShaderResourceView(
		tex.Get(),
		&resourceDescription,
		&srv
	);
	if(FAILED(result)){
		rtv.Reset();
		tex.Reset();
		OutputDebugStringA("Failed to create post effect SRV\n");
	}
}

inline void CameraPostEffect::ResizeTexture(
	ID3D11Device* device,
	const Vector2& screenSize
){
	const Vector2 normalizedSize{
		(std::max)(screenSize.x, 1.0f),
		(std::max)(screenSize.y, 1.0f)
	};

	bool sameSize =
		resolution.x == normalizedSize.x &&
		resolution.y == normalizedSize.y &&
		tex && rtv && srv;

	bool sameMipCount = true;
	if(sameSize && tex){
		D3D11_TEXTURE2D_DESC description{};
		tex->GetDesc(&description);
		sameMipCount =
			description.MipLevels ==
			static_cast<UINT>((std::max)(1, mipLevels));
	}

	if(sameSize && sameMipCount){
		return;
	}

	srv.Reset();
	rtv.Reset();
	tex.Reset();
	CreateTexture(device, normalizedSize);
}

inline void CameraPostEffect::Clear(
	ID3D11DeviceContext* context,
	const float clearColor[4]
){
	if(context && rtv && clearColor){
		context->ClearRenderTargetView(rtv.Get(), clearColor);
	}
}
