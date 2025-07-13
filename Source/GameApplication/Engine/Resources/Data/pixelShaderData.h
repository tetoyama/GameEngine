#pragma once
#include <string>
#include <d3d11.h>
#include <wrl/client.h> 
class GraphicsContext;

struct PixelShaderData
{
public:
	PixelShaderData(){
		OutputDebugStringA("Created PixelShaderData\n");
	}
	~PixelShaderData(){
		OutputDebugStringA(("Destroyed PixelShaderData: " + FilePath + "\n").c_str());
	}

	std::string FilePath = "";
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_PixelShader;
};
