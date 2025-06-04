#pragma once
#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"

#include <d3d11.h>
#include <DirectXMath.h>
#include <unordered_map>

struct MODEL
{
	const aiScene* AiScene = nullptr;

	ID3D11Buffer** VertexBuffer;
	ID3D11Buffer** IndexBuffer;

	std::unordered_map<std::string, ID3D11ShaderResourceView*> Texture;

	bool SetTexture;
};

class GraphicsContext;

void InitModel(GraphicsContext* set);

MODEL* LoadModel(const char* FileName, bool MayaBuild = true, bool SetTexture = true);
void ReleaseModel(MODEL* pModel);
void DrawModel(DirectX::XMMATRIX World, MODEL* pModel);
