// =======================================================================
// 
// meshRendererComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include <string>
#include <memory>
#include <wrl/client.h>
#include <d3d11.h>
#include "Resources/Data/textureData.h"
#include "Backends/YAMLConverters.h"

class GraphicsContext;

// メッシュ描画用のバッファデータを保持する構造体
struct MeshData {
	TextureData* textureData= nullptr;

	Microsoft::WRL::ComPtr<ID3D11Buffer>        vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>        m_IndexBuffer;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>  vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>   pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_VertexLayout;

	int meshCount = 0;
	int indexCount = 0;
};

// メッシュの描画を管理するコンポーネント
class MeshRendererComponent: public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;

		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text("MeshRendererComponent");
	}

	MeshRendererComponent() = default;
	~MeshRendererComponent() = default;

	MeshData mesh;
};
