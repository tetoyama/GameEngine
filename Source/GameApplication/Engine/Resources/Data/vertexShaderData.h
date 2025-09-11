#pragma once
#include <string>
#include <d3d11.h>
#include <wrl/client.h> 
class GraphicsContext;

struct VertexShaderData
{
public:
	VertexShaderData(){
		OutputDebugStringA("Created VertexShaderData\n");
	}
	~VertexShaderData(){
		OutputDebugStringA(("Destroyed VertexShaderData: " + FilePath + "\n").c_str());
	}

	std::string FilePath = "Asset\\Shader\\CommonVS.cso";

	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VertexShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_VertexLayout;
};
