// =======================================================================
// 
// pixelShaderData.h
// 
// =======================================================================
#pragma once
#include <string>
#include <d3d11.h>
#include <wrl/client.h> 
class GraphicsContext;

// ピクセルシェーダーデータを保持する構造体
struct PixelShaderData
{
public:
	PixelShaderData(){
		OutputDebugStringA("Created PixelShaderData\n");
	}
	~PixelShaderData(){
		OutputDebugStringA(("Destroyed PixelShaderData: " + FilePath + "\n").c_str());
	}

    std::string FilePath = "Asset\\Shader\\unlitUVTexturePS.cso";
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
};
