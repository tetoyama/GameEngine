// AudioSystem.h
#pragma once
#include "Interface/ISystem.h"
#include "GameApplication/Engine/Resources/resourceService.h"
#include "Engine/Graphics/GraphicsContext.h"
#include "Engine/Scene/scene.h"
#include "Engine/Scene/sceneManager.h"
#include "Engine/Scene/registry/ComponentRegistry.h"
#include "Component/EffectComponent.h"
#include "Engine/Audio/audioContext.h"
#include "Engine/Scene/component/transformComponent.h"
#include <Component/terrainComponent.h>


class TerrainSystem : public ISystem {
public:
	TerrainSystem(SceneContext* context)
		: m_context(context) {}

	~TerrainSystem() {}

	void Initialize() override {
		m_graphicContext = m_context->manager->graphics;
		auto entities = m_context->component->FindEntitiesWithComponent<TerrainComponent>();
		for (auto entity : entities) {

			CreateMesh(entity);
		}
	}

	void Finalize() override {
		auto entities = m_context->component->FindEntitiesWithComponent<TerrainComponent>();
		for (auto entity : entities) {
			auto* comp = m_context->component->GetComponent<TerrainComponent>(entity);
			if (comp && comp->meshRenderer) {

				comp->meshRenderer->mesh.m_IndexBuffer.Reset();
				comp->meshRenderer->mesh.m_VertexBuffer.Reset();
				comp->meshRenderer->mesh.m_PixelShader.Reset();
				comp->meshRenderer->mesh.m_VertexShader.Reset();
				comp->meshRenderer->mesh.m_VertexLayout.Reset();
				delete comp->meshRenderer->mesh.m_TextureData;

				delete comp->meshRenderer;
				comp->meshRenderer = nullptr;
			}
		}
	}

	void Start() override {

	}

	void Update(float dt) override {

	}

	void FixedUpdate(float dt) override {}
	void Draw() override {
		auto entities = m_context->component->FindEntitiesWithComponent<TerrainComponent>();
		for (auto entity : entities) {

			CreateMesh(entity);
		}
	}
	void EditorUpdate(float dt) override {

	}

private:
	SceneContext* m_context = nullptr;
	GraphicsContext* m_graphicContext = nullptr;

	void CreateMesh(Entity entity) {
		auto* comp = m_context->component->GetComponent<TerrainComponent>(entity);
		if (!comp) return;

		if (!comp->meshRenderer || comp->Scale != comp->CurrentScale) {
			if (comp->meshRenderer) {
				// 既存のメッシュがある場合は解放
				comp->meshRenderer->mesh.m_IndexBuffer.Reset();
				comp->meshRenderer->mesh.m_VertexBuffer.Reset();
			} else {
				comp->meshRenderer = new MeshRendererComponent();
			}
			if (comp->Scale + 1 != std::sqrt(comp->HeightMap.size())) {
				comp->HeightMap.resize((comp->Scale + 1) * (comp->Scale + 1), 0.0f);
			}
			// メッシュ生成
			int gridSize = comp->Scale;
			int vertexCount = (gridSize + 1) * (gridSize + 1);
			int indexCount = gridSize * gridSize * 6;
			std::vector<VERTEX_3D> vertices(vertexCount);
			std::vector<unsigned int> indices(indexCount);
			float halfSize = gridSize / 2.0f;
			float uvScale = 1.0f; // UVのスケール調整用

			// 頂点データの生成
			for (int z = 0; z <= gridSize; ++z) {
				for (int x = 0; x <= gridSize; ++x) {
					int index = z * (gridSize + 1) + x;
					vertices[index].Position = DirectX::XMFLOAT3((x - halfSize) / (float)gridSize, comp->HeightMap[x + (gridSize - z) * (gridSize + 1)], (z - halfSize) / (float)gridSize);
					vertices[index].Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
					vertices[index].Tangent = DirectX::XMFLOAT3(0.0f, 0.0, 1.0f);
					vertices[index].TexCoord = DirectX::XMFLOAT2((float)x / gridSize * uvScale, (float)z / gridSize * uvScale);
					vertices[index].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
				}
			}
			// インデックスデータの生成
			int idx = 0;
			for (int z = 0; z < gridSize; ++z) {
				for (int x = 0; x < gridSize; ++x) {
					int topLeft = z * (gridSize + 1) + x;
					int topRight = topLeft + 1;
					int bottomLeft = (z + 1) * (gridSize + 1) + x;
					int bottomRight = bottomLeft + 1;
					// 三角形1
					indices[idx++] = topLeft;
					indices[idx++] = bottomLeft;
					indices[idx++] = topRight;
					// 三角形2
					indices[idx++] = topRight;
					indices[idx++] = bottomLeft;
					indices[idx++] = bottomRight;
				}
			}
			comp->meshRenderer->mesh.meshCount = vertexCount;
			comp->meshRenderer->mesh.indexCount = indexCount;

			// 頂点バッファの作成
			D3D11_BUFFER_DESC bd{};
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(VERTEX_3D) * vertexCount;
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.CPUAccessFlags = 0;
			D3D11_SUBRESOURCE_DATA sd{};
			sd.pSysMem = vertices.data();
			m_graphicContext->GetDevice()->CreateBuffer(&bd, &sd, comp->meshRenderer->mesh.m_VertexBuffer.GetAddressOf());

			// インデックスバッファの作成
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(unsigned int) * indexCount;
			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			bd.CPUAccessFlags = 0;
			sd.pSysMem = indices.data();
			m_graphicContext->GetDevice()->CreateBuffer(&bd, &sd, comp->meshRenderer->mesh.m_IndexBuffer.GetAddressOf());

			// シェーダーのロード
			comp->meshRenderer->mesh.m_VertexShader = nullptr;
			comp->meshRenderer->mesh.m_PixelShader = nullptr;
			comp->meshRenderer->mesh.m_VertexLayout = nullptr;

			//m_graphicContext->CreateVertexShader("Asset\\Shader\\commonVS.cso", comp->meshRenderer->mesh.m_VertexShader.GetAddressOf(), comp->meshRenderer->mesh.m_VertexLayout.GetAddressOf());
			//m_graphicContext->CreatePixelShader("Asset\\Shader\\unlitUVTexturePS.cso", comp->meshRenderer->mesh.m_PixelShader.GetAddressOf());

			comp->CurrentScale = comp->Scale;
		}

	}
};
