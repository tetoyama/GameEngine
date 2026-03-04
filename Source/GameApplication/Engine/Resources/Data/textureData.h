// =======================================================================
// 
// textureData.h
// 
// =======================================================================
#pragma once
#include <string>
#include <d3d11.h>
#include <wrl/client.h> 

// テクスチャリソースのデータを保持する構造体
struct TextureData {
	TextureData(){
		OutputDebugStringA("Created TextureData\n");
	}
	~TextureData(){
		OutputDebugStringA(("Destroyed TextureData: " + FilePath + "\n").c_str());
		pTexture.Reset();
	}
	std::string FilePath;
	Microsoft::WRL::ComPtr <ID3D11ShaderResourceView> pTexture;	//ポインター
	int Width = 0;
	int Height = 0;
};
