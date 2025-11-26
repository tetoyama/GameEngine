#include "renderTarget.h"
#include "Engine/Graphics/graphicsContext.h"

RenderTarget::RenderTarget(const Vector2& _size, GraphicsContext* _graphicsContext, const RenderTargetType& setType){
	type = setType;
	Resize(_size, _graphicsContext);
}

void RenderTarget::Resize(const Vector2& _size, GraphicsContext* _graphicsContext){
	if(_size == size){
		return;
	}
	size = _size;

	D3D11_TEXTURE2D_DESC td = {};
	td.Width = (int)_size.x; td.Height = (int)_size.y;
	td.MipLevels = 1; td.ArraySize = 1;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;

	if(type == RENDERTARGET_TYPE_DEPTH){
		// 深度専用テクスチャ
		td.Format = DXGI_FORMAT_R32_TYPELESS;
		td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	} else{
		// カラー
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	}

	GraphicsContext* graphicsContext = _graphicsContext;
	ID3D11Device* device = graphicsContext->GetDevice();

	HRESULT hr = device->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
	if(FAILED(hr) || !tex) return;

	// RenderTargetView はカラー用でも作っておく
	if(type != RENDERTARGET_TYPE_DEPTH){
		device->CreateRenderTargetView(tex.Get(), nullptr, rtv.ReleaseAndGetAddressOf());
	}

	// ShaderResourceView 作成
	if(type == RENDERTARGET_TYPE_DEPTH){
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // SRV 用に変換
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(tex.Get(), &srvDesc, srv.ReleaseAndGetAddressOf());
	} else{
		device->CreateShaderResourceView(tex.Get(), nullptr, srv.ReleaseAndGetAddressOf());
	}

	// デプスステンシルバッファ作成
	ID3D11Texture2D* depthStencilTex = nullptr;
	D3D11_TEXTURE2D_DESC depthDesc{};
	depthDesc.Width = (int)_size.x;
	depthDesc.Height = (int)_size.y;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.SampleDesc.Quality = 0;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthDesc.CPUAccessFlags = 0;
	depthDesc.MiscFlags = 0;
	depthDesc.Format = (type == RENDERTARGET_TYPE_DEPTH) ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

	hr = device->CreateTexture2D(&depthDesc, nullptr, &depthStencilTex);
	if(FAILED(hr) || !depthStencilTex) return;

	// DSV 作成
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = depthDesc.Format;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = 0;
	hr = device->CreateDepthStencilView(depthStencilTex, &dsvDesc, dsv.ReleaseAndGetAddressOf());
	depthStencilTex->Release();
}

void RenderTarget::Clear(ID3D11DeviceContext* ctx, const float clearColor[4]) const{
	if(rtv){
		ctx->ClearRenderTargetView(rtv.Get(), clearColor);
	}
	if(dsv){
		ctx->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}
}
