// =======================================================================
// 
// renderTarget.h
// 
// =======================================================================
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "Backends/myVector2.h"

class GraphicsContext;

// レンダーターゲットの種別を定義する列挙型
enum RenderTargetType
{
	RENDERTARGET_TYPE_COLOR,
	RENDERTARGET_TYPE_COLOR_UNORM,
	RENDERTARGET_TYPE_COLOR_NO_DSV,
	RENDERTARGET_TYPE_DEPTH,
	RENDERTARGET_TYPE_UINT4,
};

// テクスチャ・ビューを持つレンダーターゲット
struct RenderTarget {

	RenderTarget(const Vector2& _size, GraphicsContext* _graphicsContext,const RenderTargetType& type);

	Microsoft::WRL::ComPtr<ID3D11Texture2D>				tex = nullptr;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>		rtv = nullptr;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	srv = nullptr;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>		dsv = nullptr;

	Vector2 size = Vector2(-1, -1);
	RenderTargetType type = RENDERTARGET_TYPE_COLOR;

	void Resize(const Vector2& _size, GraphicsContext* _graphicsContext);

	void Clear(ID3D11DeviceContext* ctx, const float clearColor[4]) const;
};
