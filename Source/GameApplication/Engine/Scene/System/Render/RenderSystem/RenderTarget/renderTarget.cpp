// =======================================================================
// 
// renderTarget.cpp
// 
// =======================================================================
#include "renderTarget.h"
#include "Graphics/graphicsContext.h"
#include <algorithm>
RenderTarget::RenderTarget(const Vector2& _size, GraphicsContext* _graphicsContext, const RenderTargetType& setType){
	type = setType;
	Resize(_size, _graphicsContext);
}

void RenderTarget::Resize(const Vector2& _size, GraphicsContext* _graphicsContext) {
    if (_size == size) return;
    size = _size;

    D3D11_TEXTURE2D_DESC m_Td= {};
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

                HRESULT m_Hr= device->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
                if (FAILED(hr) || !tex) return;

                device->CreateRenderTargetView(tex.Get(), nullptr, rtv.ReleaseAndGetAddressOf());
                device->CreateShaderResourceView(tex.Get(), nullptr, srv.ReleaseAndGetAddressOf());

                // 深度ステンシルを別で作成（カラーRT用）
                ID3D11Texture2D* depthStencilTex = nullptr;

                D3D11_TEXTURE2D_DESC m_DepthDesc= td;
                depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
                depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
                depthDesc.MiscFlags = 0;

                hr = device->CreateTexture2D(&depthDesc, nullptr, &depthStencilTex);
                if (FAILED(hr) || !depthStencilTex) return;

                D3D11_DEPTH_STENCIL_VIEW_DESC m_DsvDesc{};
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

                HRESULT m_Hr= device->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
                if (FAILED(hr) || !tex) return;

                device->CreateRenderTargetView(tex.Get(), nullptr, rtv.ReleaseAndGetAddressOf());
                device->CreateShaderResourceView(tex.Get(), nullptr, srv.ReleaseAndGetAddressOf());

                // 深度ステンシルを別で作成（カラーRT用）
                ID3D11Texture2D* depthStencilTex = nullptr;

                D3D11_TEXTURE2D_DESC m_DepthDesc= td;
                depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
                depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
                depthDesc.MiscFlags = 0;

                hr = device->CreateTexture2D(&depthDesc, nullptr, &depthStencilTex);
                if (FAILED(hr) || !depthStencilTex) return;

                D3D11_DEPTH_STENCIL_VIEW_DESC m_DsvDesc{};
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

                HRESULT m_Hr= device->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
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

                HRESULT m_Hr= device->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
                if (FAILED(hr) || !tex) return;

                // DSV 作成（tex に対して作る）
                D3D11_DEPTH_STENCIL_VIEW_DESC m_DsvDesc{};
                dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
                dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

                hr = device->CreateDepthStencilView(tex.Get(), &dsvDesc, dsv.ReleaseAndGetAddressOf());
                if (FAILED(hr)) return;

                // SRV 作成（tex に対して作る）
                D3D11_SHADER_RESOURCE_VIEW_DESC m_SrvDesc{};
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

                HRESULT m_Hr= device->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
                if (FAILED(hr) || !tex) return;

                D3D11_RENDER_TARGET_VIEW_DESC m_RtvDesc{};
                rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

                device->CreateRenderTargetView(tex.Get(), &rtvDesc, rtv.ReleaseAndGetAddressOf());

                D3D11_SHADER_RESOURCE_VIEW_DESC m_SrvDesc{};
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

PickResult RenderTarget::Pick(const Vector2& uv, GraphicsContext* graphicsContext){
	PickResult m_Result;
	if(!tex) return result;

	ID3D11Device* device = graphicsContext->GetDevice();
	ID3D11DeviceContext* context = graphicsContext->GetDeviceContext();

	// 1. テクスチャ情報の取得
	D3D11_TEXTURE2D_DESC m_Desc;
	tex->GetDesc(&desc);

	// 2. ピクセル座標の計算
	UINT m_PixelX= (UINT)std::clamp(uv.x * size.x, 0.0f, size.x - 1.0f);
	UINT m_PixelY= (UINT)std::clamp(uv.y * size.y, 0.0f, size.y - 1.0f);

	// 3. 1x1のステージングテクスチャ作成
	D3D11_TEXTURE2D_DESC m_StagingDesc= desc;
	stagingDesc.Width = 1;
	stagingDesc.Height = 1;
	stagingDesc.MipLevels = 1;
	stagingDesc.ArraySize = 1;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_StagingTex;
	HRESULT m_Hr= device->CreateTexture2D(&stagingDesc, nullptr, stagingTex.GetAddressOf());
	if(FAILED(hr)) return result;

	// 4. 指定座標の1ピクセルをコピー
	D3D11_BOX m_SrcBox= {pixelX, pixelY, 0, pixelX + 1, pixelY + 1, 1};
	context->CopySubresourceRegion(stagingTex.Get(), 0, 0, 0, 0, tex.Get(), 0, &srcBox);

	// 5. マップしてデータ取得
	D3D11_MAPPED_SUBRESOURCE m_Mapped;
	hr = context->Map(stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
	if(SUCCEEDED(hr)){

		switch(type){
			case RENDERTARGET_TYPE_COLOR:
			case RENDERTARGET_TYPE_COLOR_NO_DSV:
			{
				// DXGI_FORMAT_R16G16B16A16_FLOAT の場合
				// CPUでは float16 を直接扱えないため HALF から float へ変換
				const DirectX::PackedVector::HALF* pHalf = reinterpret_cast<const DirectX::PackedVector::HALF*>(mapped.pData);
				result.f[0] = DirectX::PackedVector::XMConvertHalfToFloat(pHalf[0]);
				result.f[1] = DirectX::PackedVector::XMConvertHalfToFloat(pHalf[1]);
				result.f[2] = DirectX::PackedVector::XMConvertHalfToFloat(pHalf[2]);
				result.f[3] = DirectX::PackedVector::XMConvertHalfToFloat(pHalf[3]);
				break;
			}
			case RENDERTARGET_TYPE_COLOR_UNORM:
			{
				// DXGI_FORMAT_R8G8B8A8_UNORM の場合
				const uint8_t* pByte = reinterpret_cast<const uint8_t*>(mapped.pData);
				result.b[0] = pByte[0];
				result.b[1] = pByte[1];
				result.b[2] = pByte[2];
				result.b[3] = pByte[3];
				break;
			}
			case RENDERTARGET_TYPE_UINT4:
			{
				// DXGI_FORMAT_R32G32B32A32_UINT の場合
				const uint32_t* pUint = reinterpret_cast<const uint32_t*>(mapped.pData);
				result.u[0] = pUint[0];
				result.u[1] = pUint[1];
				result.u[2] = pUint[2];
				result.u[3] = pUint[3];
				break;
			}
			case RENDERTARGET_TYPE_DEPTH:
			{
				// DXGI_FORMAT_D32_FLOAT の場合 (SRV経由の読み取り想定)
				result.f[0] = *reinterpret_cast<const float*>(mapped.pData);
				break;
			}
		}

		context->Unmap(stagingTex.Get(), 0);
	}

	return m_Result;
}
