#pragma once
#include <string>
#include <d3d11.h>
#include <DirectXMath.h>
#include <unordered_map>

struct aiScene;

struct ModelData
{
	~ModelData();

	const aiScene* AiScene = nullptr;

	ID3D11Buffer** VertexBuffer;
	ID3D11Buffer** IndexBuffer;

	std::unordered_map<std::string, ID3D11ShaderResourceView*> Texture;

	bool SetTexture;
};
