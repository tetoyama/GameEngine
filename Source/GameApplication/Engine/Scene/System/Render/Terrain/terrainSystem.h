// =======================================================================
// 
// terrainSystem.h
// 
// =======================================================================
#pragma once
#include "Interface/ISystem.h"
#include "Resources/resourceService.h"
#include "Graphics/GraphicsContext.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/registry/ComponentRegistry.h"
#include "Component/EffectComponent.h"
#include "Audio/audioContext.h"
#include "Scene/component/transformComponent.h"
#include <Component/terrainComponent.h>
#include <Component/ColliderComponent.h>


class TerrainSystem : public ISystem {
public:
	const char* GetSystemName() const override{
		return "TerrainSystem";
	}

	TerrainSystem(SceneManagerContext* context)
		: m_context(context) {}

	~TerrainSystem() {}

	void Initialize() override {
		m_graphicContext = m_context->graphics;
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<TerrainComponent>();
			for (auto entity : entities) {

				CreateMesh(context,entity);
			}
		}
	}

	void Finalize() override {
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<TerrainComponent>();
			for (auto entity : entities) {
				auto* comp = context->component->GetComponent<TerrainComponent>(entity);
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
	}
	void Draw() override {
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<TerrainComponent>();
			for (auto entity : entities) {

				CreateMesh(context,entity);
			}
		}
	}

private:
	SceneManagerContext* m_context = nullptr;
	GraphicsContext* m_graphicContext = nullptr;

	void CreateMesh(SceneContext* context, Entity entity){
		auto* comp = context->component->GetComponent<TerrainComponent>(entity);
		if(!comp) return;

		if(!comp->meshRenderer || comp->Scale != comp->CurrentScale){
			if(!comp->meshRenderer)
				comp->meshRenderer = new MeshRendererComponent();
			else{
				comp->meshRenderer->mesh.m_VertexBuffer.Reset();
				comp->meshRenderer->mesh.m_IndexBuffer.Reset();
			}

			int gridSize = comp->Scale;
			int vertexCount = (gridSize + 1) * (gridSize + 1);
			int indexCount = gridSize * gridSize * 6;

			std::vector<VERTEX_3D> vertices(vertexCount);
			std::vector<unsigned int> indices(indexCount);

			float halfSize = gridSize * 0.5f;

			/*============================
			  頂点生成（法線は0初期化）
			============================*/
			for(int z = 0; z <= gridSize; ++z){
				for(int x = 0; x <= gridSize; ++x){
					int i = z * (gridSize + 1) + x;

					if ((int)comp->HeightMap.size() >= vertexCount) {
						vertices[i].Position = {
							(x - halfSize) / gridSize,
							comp->HeightMap[x + (gridSize - z) * (gridSize + 1)],
							(z - halfSize) / gridSize
						};
					} else {
						vertices[i].Position = {
							(x - halfSize) / gridSize,
							0.0f,
							(z - halfSize) / gridSize
						};
					}


					vertices[i].Normal = {0,0,0};
					vertices[i].Tangent = {1,0,0};
					vertices[i].TexCoord = {
						(float)x / gridSize,
						(float)z / gridSize
					};
					vertices[i].Diffuse = {1,1,1,1};
				}
			}

			/*============================
			  インデックス生成
			============================*/
			int idx = 0;
			for(int z = 0; z < gridSize; ++z){
				for(int x = 0; x < gridSize; ++x){
					int tl = z * (gridSize + 1) + x;
					int tr = tl + 1;
					int bl = (z + 1) * (gridSize + 1) + x;
					int br = bl + 1;

					indices[idx++] = tl;
					indices[idx++] = bl;
					indices[idx++] = tr;

					indices[idx++] = tr;
					indices[idx++] = bl;
					indices[idx++] = br;
				}
			}

			/*============================
			  法線計算（面法線加算）
			============================*/
			for(int i = 0; i < indexCount; i += 3){
				auto& v0 = vertices[indices[i + 0]];
				auto& v1 = vertices[indices[i + 1]];
				auto& v2 = vertices[indices[i + 2]];

				DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&v0.Position);
				DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&v1.Position);
				DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&v2.Position);

				DirectX::XMVECTOR e1 = DirectX::XMVectorSubtract(p1, p0);
				DirectX::XMVECTOR e2 = DirectX::XMVectorSubtract(p2, p0);

				DirectX::XMVECTOR n = DirectX::XMVector3Cross(e1, e2);

				DirectX::XMFLOAT3 normal;
				DirectX::XMStoreFloat3(&normal, n);

				v0.Normal.x += normal.x;
				v0.Normal.y += normal.y;
				v0.Normal.z += normal.z;

				v1.Normal.x += normal.x;
				v1.Normal.y += normal.y;
				v1.Normal.z += normal.z;

				v2.Normal.x += normal.x;
				v2.Normal.y += normal.y;
				v2.Normal.z += normal.z;
			}

			/*============================
			  法線正規化
			============================*/
			for(auto& v : vertices){
				DirectX::XMVECTOR n = DirectX::XMLoadFloat3(&v.Normal);
				n = DirectX::XMVector3Normalize(n);
				DirectX::XMStoreFloat3(&v.Normal, n);
			}

			/*============================
			  バッファ作成
			============================*/
			D3D11_BUFFER_DESC bd{};
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.ByteWidth = sizeof(VERTEX_3D) * vertexCount;

			D3D11_SUBRESOURCE_DATA sd{};
			sd.pSysMem = vertices.data();

			m_graphicContext->GetDevice()->CreateBuffer(
				&bd, &sd,
				comp->meshRenderer->mesh.m_VertexBuffer.GetAddressOf());

			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			bd.ByteWidth = sizeof(unsigned int) * indexCount;
			sd.pSysMem = indices.data();

			m_graphicContext->GetDevice()->CreateBuffer(
				&bd, &sd,
				comp->meshRenderer->mesh.m_IndexBuffer.GetAddressOf());

			comp->meshRenderer->mesh.meshCount = vertexCount;
			comp->meshRenderer->mesh.indexCount = indexCount;
			comp->CurrentScale = comp->Scale;

			// ビジュアルメッシュを再構築したので物理コライダも再構築を要求する
			// （HeightMapの頂点位置とコライダの頂点位置が一致するよう同期させる）
			auto* col = context->component->GetComponent<ColliderComponent>(entity);
			if (col) col->needsUpdate = true;
		}
	}
};
