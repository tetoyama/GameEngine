#pragma once
#include <string>
#include <d3d11.h>
#include <wrl/client.h> 


struct TextureData {

	std::string FilePath;
	Microsoft::WRL::ComPtr <ID3D11ShaderResourceView> pTexture;	//ポインター
	int Width = 0;
	int Height = 0;
};
