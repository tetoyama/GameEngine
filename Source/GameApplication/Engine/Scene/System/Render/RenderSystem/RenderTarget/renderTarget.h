#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "Backends/myVector2.h"

class GraphicsContext;

enum RenderTargetType
{
	RENDERTARGET_TYPE_COLOR,
	RENDERTARGET_TYPE_COLOR_UNORM,
	RENDERTARGET_TYPE_COLOR_NO_DSV,
	RENDERTARGET_TYPE_DEPTH,
	RENDERTARGET_TYPE_UINT4,
};

struct RenderTarget {

	RenderTarget(const Vector2& _size, GraphicsContext* _graphicsContext,const RenderTargetType& type);

	Microsoft::WRL::ComPtr<ID3D11Texture2D>				tex;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>		rtv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	srv;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>		dsv;

	Vector2 size = Vector2(-1, -1);
	RenderTargetType type = RENDERTARGET_TYPE_COLOR;

	void Resize(const Vector2& _size, GraphicsContext* _graphicsContext);

	void Clear(ID3D11DeviceContext* ctx, const float clearColor[4]) const;
};
