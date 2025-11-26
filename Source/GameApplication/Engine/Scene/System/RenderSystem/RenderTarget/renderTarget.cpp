#include "renderTarget.h"
#include "Engine/Graphics/graphicsContext.h"

RenderTarget::RenderTarget(const Vector2& _size, GraphicsContext* _graphicsContext){
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
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	GraphicsContext* graphicsContext = _graphicsContext;
	ID3D11Device* device = graphicsContext->GetDevice();

	device->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
	if(tex.Get()){
		device->CreateRenderTargetView(tex.Get(), nullptr, rtv.ReleaseAndGetAddressOf());
		device->CreateShaderResourceView(tex.Get(), nullptr, srv.ReleaseAndGetAddressOf());
	}

	// デプスステンシルバッファ作成
	ID3D11Texture2D* depthStencile{};
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = (int)_size.x;
	textureDesc.Height = (int)_size.y;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;
	HRESULT hr = device->CreateTexture2D(&textureDesc, NULL, &depthStencile);
	if(FAILED(hr)){
		return;
	}

	// デプスステンシルビュー作成
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
	depthStencilViewDesc.Format = textureDesc.Format;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Flags = 0;
	if(depthStencile){
		hr = device->CreateDepthStencilView(depthStencile, &depthStencilViewDesc, dsv.ReleaseAndGetAddressOf());
		depthStencile->Release();

		if(FAILED(hr)){
			return;
		}
	}
}

void RenderTarget::Clear(ID3D11DeviceContext* ctx, const float clearColor[4]) const{
	if(rtv){
		ctx->ClearRenderTargetView(rtv.Get(), clearColor);
	}
	if(dsv){
		ctx->ClearDepthStencilView(
			dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}
}
