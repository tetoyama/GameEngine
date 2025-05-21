// Engine/Scene/Component/meshRendererComponent.h
#pragma once
#include "Component.h"
#include <string>
#include <memory>
#include <wrl/client.h>
#include <d3d11.h>

// 仮のMesh/Material構造体（実際は専用クラスやリソース参照に拡張してください）
struct MeshData {
public:

	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
	UINT indexCount = 0;
};

struct Material {
	// テクスチャ、シェーダ、色など
	// ここではスケルトンなので空
};

class MeshRendererComponent: public Component {
public:
	MeshRendererComponent() = default;
	virtual ~MeshRendererComponent() = default;

	std::shared_ptr<MeshData> mesh;
	std::shared_ptr<Material> material;
	// 必要に応じて他の描画パラメータも追加
};
