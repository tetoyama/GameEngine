#pragma once
#include <string>
#include <d3d11.h>
#include <wrl/client.h> 
class GraphicsContext;

struct VertexShaderData
{
public:
	std::string FilePath = "";

	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VertexShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_VertexLayout;
};
