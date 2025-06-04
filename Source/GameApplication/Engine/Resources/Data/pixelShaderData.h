#pragma once
#include <string>
#include <d3d11.h>
#include <wrl/client.h> 
class GraphicsContext;

struct PixelShaderData
{
public:
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_PixelShader;
};
