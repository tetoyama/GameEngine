#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "Backends/myVector2.h"

class GraphicsContext;

struct RenderTarget {

	RenderTarget(const Vector2& _size, GraphicsContext* _graphicsContext);

	Microsoft::WRL::ComPtr<ID3D11Texture2D>				tex;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>		rtv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	srv;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>		dsv;

	Vector2 size = Vector2(-1, -1);

	void Resize(const Vector2& _size, GraphicsContext* _graphicsContext);

	void Clear(ID3D11DeviceContext* ctx, const float clearColor[4]) const;
};
