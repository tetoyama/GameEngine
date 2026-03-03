// =======================================================================
// 
// renderTarget.cpp
// 
// =======================================================================
#include "renderTarget.h"
#include "Graphics/graphicsContext.h"

RenderTarget::RenderTarget(const Vector2& _size, GraphicsContext* _graphicsContext, const RenderTargetType& setType){
	type = setType;
	Resize(_size, _graphicsContext);
}

void RenderTarget::Resize(const Vector2& _size, GraphicsContext* _graphicsContext) {
    if (_size == size) return;
    size = _size;

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = (UINT)_size.x;
    td.Height = (UINT)_size.y;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;

    GraphicsContext* graphicsContext = _graphicsContext;
    ID3D11Device* device = graphicsContext->GetDevice();

    switch (type) {
        case RENDERTARGET_TYPE_COLOR:
            {
                // --- カラーターゲットの場合 ---
                td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

                HRESULT hr = device->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
                if (FAILED(hr) || !tex) return;

                device->CreateRenderTargetView(tex.Get(), nullptr, rtv.ReleaseAndGetAddressOf());
                device->CreateShaderResourceView(tex.Get(), nullptr, srv.ReleaseAndGetAddressOf());

                // 深度ステンシルを別で作成（カラーRT用）
                ID3D11Texture2D* depthStencilTex = nullptr;

                D3D11_TEXTURE2D_DESC depthDesc = td;
                depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
                depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
                depthDesc.MiscFlags = 0;

                hr = device->CreateTexture2D(&depthDesc, nullptr, &depthStencilTex);
                if (FAILED(hr) || !depthStencilTex) return;

                D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
                dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
                dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

                device->CreateDepthStencilView(depthStencilTex, &dsvDesc, dsv.ReleaseAndGetAddressOf());
                depthStencilTex->Release();
            }
            break;
        case RENDERTARGET_TYPE_COLOR_UNORM:
            {
                // --- カラーターゲットの場合 ---
                td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

                HRESULT hr = device->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
                if (FAILED(hr) || !tex) return;

                device->CreateRenderTargetView(tex.Get(), nullptr, rtv.ReleaseAndGetAddressOf());
                device->CreateShaderResourceView(tex.Get(), nullptr, srv.ReleaseAndGetAddressOf());

                // 深度ステンシルを別で作成（カラーRT用）
                ID3D11Texture2D* depthStencilTex = nullptr;

                D3D11_TEXTURE2D_DESC depthDesc = td;
                depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
                depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
                depthDesc.MiscFlags = 0;

                hr = device->CreateTexture2D(&depthDesc, nullptr, &depthStencilTex);
                if (FAILED(hr) || !depthStencilTex) return;

                D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
                dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
                dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

                device->CreateDepthStencilView(depthStencilTex, &dsvDesc, dsv.ReleaseAndGetAddressOf());
                depthStencilTex->Release();
            }
			break;
        case RENDERTARGET_TYPE_COLOR_NO_DSV:
            {
                td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

                HRESULT hr = device->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
                if (FAILED(hr) || !tex) return;

                device->CreateRenderTargetView(tex.Get(), nullptr, rtv.ReleaseAndGetAddressOf());
                device->CreateShaderResourceView(tex.Get(), nullptr, srv.ReleaseAndGetAddressOf());

                dsv.Reset();
            }
            break;
		case RENDERTARGET_TYPE_DEPTH:
            {
                // --- 深度シャドウマップの場合 ---
                td.Format = DXGI_FORMAT_R32_TYPELESS;
                td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

                HRESULT hr = device->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
                if (FAILED(hr) || !tex) return;

                // DSV 作成（tex に対して作る）
                D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
                dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
                dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

                hr = device->CreateDepthStencilView(tex.Get(), &dsvDesc, dsv.ReleaseAndGetAddressOf());
                if (FAILED(hr)) return;

                // SRV 作成（tex に対して作る）
                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = 1;

                hr = device->CreateShaderResourceView(tex.Get(), &srvDesc, srv.ReleaseAndGetAddressOf());
                if (FAILED(hr)) return;

                // ※ この場合 RTV は不要
                rtv.Reset();
            }
			break;
        case RENDERTARGET_TYPE_UINT4:
            {
				// --- UINT4 ターゲットの場合 ---
                td.Format = DXGI_FORMAT_R32G32B32A32_UINT;
                td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

                HRESULT hr = device->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
                if (FAILED(hr) || !tex) return;

                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
                rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

                device->CreateRenderTargetView(tex.Get(), &rtvDesc, rtv.ReleaseAndGetAddressOf());

                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                srvDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = 1;

                device->CreateShaderResourceView(tex.Get(), &srvDesc, srv.ReleaseAndGetAddressOf());

                dsv.Reset();
            }
            break;
    }
}

void RenderTarget::Clear(ID3D11DeviceContext* ctx, const float clearColor[4]) const{
	if(rtv){
		ctx->ClearRenderTargetView(rtv.Get(), clearColor);
	}
	if(dsv){
		ctx->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}
}
