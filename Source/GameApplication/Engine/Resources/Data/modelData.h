#pragma once
#include <string>
#include <d3d11.h>
#include <DirectXMath.h>
#include <unordered_map>

struct aiScene;

struct ModelData
{
public:
	ModelData(){
		OutputDebugStringA("Created ModelData\n");
	}
	~ModelData(){
		OutputDebugStringA(("Destroyed ModelData: " + FilePath + "\n").c_str());
		Release();
	}

	void Release();

	std::string FilePath = "";
	
	const aiScene* AiScene = nullptr;

	ID3D11Buffer** VertexBuffer = nullptr;
	ID3D11Buffer** IndexBuffer = nullptr;

	std::unordered_map<std::string, ID3D11ShaderResourceView*> Texture;

	bool isBlender = false;

	bool SetTexture = false;
};
