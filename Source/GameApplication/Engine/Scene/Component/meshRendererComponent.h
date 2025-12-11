// Scene/Component/meshRendererComponent.h
#pragma once
#include "Interface/IComponent.h"
#include <string>
#include <memory>
#include <wrl/client.h>
#include <d3d11.h>
#include "Resources/Data/textureData.h"
#include "BackEnds/YAMLConverters.h"

class GraphicsContext;

struct MeshData {
	TextureData* m_TextureData = nullptr;

	Microsoft::WRL::ComPtr<ID3D11Buffer>        m_VertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>        m_IndexBuffer;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>  m_VertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_PixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_VertexLayout;

	int meshCount = 0;
	int indexCount = 0;
};

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
