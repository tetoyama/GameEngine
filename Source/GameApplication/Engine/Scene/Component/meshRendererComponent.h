// =======================================================================
// 
// meshRendererComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include <cstdint>
#include <string>
#include <memory>
#include <wrl/client.h>
#include <d3d11.h>
#include "Resources/Data/textureData.h"
#include "Backends/YAMLConverters.h"
#include "Backends/myVector3.h"

class GraphicsContext;

// メッシュ描画用のバッファデータを保持する構造体
struct MeshData {
	TextureData* m_TextureData = nullptr;

	Microsoft::WRL::ComPtr<ID3D11Buffer>        m_VertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>        m_IndexBuffer;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>  m_VertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_PixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_VertexLayout;

	int meshCount = 0;
	int indexCount = 0;

	// GPU Bufferから頂点を逆読みしないため、Mesh生成側がCPU頂点列から
	// 同時に登録するDerived Metadata。Scene YAMLへは保存しない。
	Vector3 localBoundsMin{};
	Vector3 localBoundsMax{};
	std::uint64_t localBoundsRevision = 0;
	std::uint64_t geometryResourceKey = 0;
	bool localBoundsValid = false;

	void SetLocalBounds(
		const Vector3& minimum,
		const Vector3& maximum
	) noexcept {
		localBoundsMin = minimum;
		localBoundsMax = maximum;
		++localBoundsRevision;
		if(localBoundsRevision == 0) ++localBoundsRevision;
		localBoundsValid = true;
	}

	void SetGeometryResourceKey(std::uint64_t key) noexcept {
		geometryResourceKey = key;
	}

	void ClearLocalBounds() noexcept {
		localBoundsMin = Vector3{};
		localBoundsMax = Vector3{};
		geometryResourceKey = 0;
		++localBoundsRevision;
		if(localBoundsRevision == 0) ++localBoundsRevision;
		localBoundsValid = false;
	}
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