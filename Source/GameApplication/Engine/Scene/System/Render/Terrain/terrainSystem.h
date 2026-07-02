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


// 地形メッシュの生成・更新を管理するシステム
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

	void RegisterTasks(SystemScheduleBuilder& builder) override {
		builder.AddTask(
			"TerrainSystem.Mesh.Upload",
			SystemTaskDomain::Render,
			SystemPhase::Early,
			0,
			SystemAccess::LegacyExclusive(),
			ThreadAffinity::MainThread,
			[this](const SystemTaskContext&){
				Draw();
			}
		);
	}

	void Draw() {
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

    void ComputeNormalsAndTangents(std::vector<VERTEX_3D>& vertices, const std::vector<unsigned int>& indices, bool invertNormals = false) {
        // 法線をゼロ初期化（念のため）
        for (auto& v : vertices) {
            v.Normal = { 0.0f, 0.0f, 0.0f };
            v.Tangent = { 0.0f, 0.0f, 0.0f };
        }

        const size_t indexCount = indices.size();
        for (size_t i = 0; i + 2 < indexCount; i += 3) {
            unsigned int i0 = indices[i + 0];
            unsigned int i1 = indices[i + 1];
            unsigned int i2 = indices[i + 2];

            // 頂点読み出し
            DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&vertices[i0].Position);
            DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&vertices[i1].Position);
            DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&vertices[i2].Position);

            // 辺ベクトル（順序によって法線の向きが決まる）
            DirectX::XMVECTOR e1 = DirectX::XMVectorSubtract(p1, p0);
            DirectX::XMVECTOR e2 = DirectX::XMVectorSubtract(p2, p0);

            // 面法線（面積に比例する大きさ）
            DirectX::XMVECTOR faceNormal = DirectX::XMVector3Cross(e1, e2);

            if (invertNormals) {
                faceNormal = DirectX::XMVectorNegate(faceNormal);
            }

            // faceNormal を各頂点に加算（面積重み付け）
            DirectX::XMFLOAT3 fn;
            DirectX::XMStoreFloat3(&fn, faceNormal);

            vertices[i0].Normal.x += fn.x;
            vertices[i0].Normal.y += fn.y;
            vertices[i0].Normal.z += fn.z;

            vertices[i1].Normal.x += fn.x;
            vertices[i1].Normal.y += fn.y;
            vertices[i1].Normal.z += fn.z;

            vertices[i2].Normal.x += fn.x;
            vertices[i2].Normal.y += fn.y;
            vertices[i2].Normal.z += fn.z;

            // 簡易タンジェント（必要なら詳細計算に置換）
            // ここでは仮に面に沿ったタンジェントを追加する（正規化は後で）
            DirectX::XMFLOAT3 tan;
            DirectX::XMStoreFloat3(&tan, e1);
            vertices[i0].Tangent.x += tan.x;
            vertices[i0].Tangent.y += tan.y;
            vertices[i0].Tangent.z += tan.z;

            vertices[i1].Tangent.x += tan.x;
            vertices[i1].Tangent.y += tan.y;
            vertices[i1].Tangent.z += tan.z;

            vertices[i2].Tangent.x += tan.x;
            vertices[i2].Tangent.y += tan.y;
            vertices[i2].Tangent.z += tan.z;
        }

        // 正規化（長さが小さい場合は上方向を代入）
        for (auto& v : vertices) {
            DirectX::XMVECTOR n = DirectX::XMLoadFloat3(&v.Normal);
            float len = DirectX::XMVectorGetX(DirectX::XMVector3Length(n));
            if (len < 1e-6f) {
                // 異常な場合は上向きにフォールバック
                v.Normal = { 0.0f, 1.0f, 0.0f };
            } else {
                n = DirectX::XMVector3Normalize(n);
                DirectX::XMStoreFloat3(&v.Normal, n);
            }

            // タンジェント正規化（ゼロチェック）
            DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&v.Tangent);
            float tlen = DirectX::XMVectorGetX(DirectX::XMVector3Length(t));
            if (tlen < 1e-6f) {
                v.Tangent = { 1.0f, 0.0f, 0.0f };
            } else {
                t = DirectX::XMVector3Normalize(t);
                DirectX::XMStoreFloat3(&v.Tangent, t);
            }
        }
    }

    void CreateMesh(SceneContext* context, Entity entity) {
        auto* comp = context->component->GetComponent<TerrainComponent>(entity);
        if (!comp) return;

        if (!comp->meshRenderer || comp->Scale != comp->CurrentScale) {
            if (!comp->meshRenderer)
                comp->meshRenderer = new MeshRendererComponent();
            else {
                comp->meshRenderer->mesh.m_VertexBuffer.Reset();
                comp->meshRenderer->mesh.m_IndexBuffer.Reset();
            }

            int gridSize = comp->Scale;
            int vertexCount = (gridSize + 1) * (gridSize + 1);
            int indexCount = gridSize * gridSize * 6;

            std::vector<VERTEX_3D> vertices(vertexCount);
            std::vector<unsigned int> indices(indexCount);

            float halfSize = gridSize * 0.5f;

            // 頂点生成
            for (int z = 0; z <= gridSize; ++z) {
                for (int x = 0; x <= gridSize; ++x) {
                    int i = z * (gridSize + 1) + x;
                    float vx = (x - halfSize) / (float)gridSize;
                    float vz = (z - halfSize) / (float)gridSize;
                    float vy = 0.0f;
                    if ((int)comp->HeightMap.size() >= vertexCount) {
                        // HeightMap の行方向をどちらに定義しているかでインデックスを調整すること
                        vy = comp->HeightMap[x + (gridSize - z) * (gridSize + 1)];
                    }
                    vertices[i].Position = { vx, vy, vz };
                    vertices[i].Normal = { 0.0f, 0.0f, 0.0f };
                    vertices[i].Tangent = { 1.0f, 0.0f, 0.0f };
                    vertices[i].TexCoord = { (float)x / gridSize, (float)z / gridSize };
                    vertices[i].Diffuse = { 1, 1, 1, 1 };
                }
            }

            // インデックス生成（ここで winding を明示：CCW を採用）
            int idx = 0;
            for (int z = 0; z < gridSize; ++z) {
                for (int x = 0; x < gridSize; ++x) {
                    int tl = z * (gridSize + 1) + x;
                    int tr = tl + 1;
                    int bl = (z + 1) * (gridSize + 1) + x;
                    int br = bl + 1;

                    // 三角形 1 : tl, bl, tr  (この順序が CCW か確認すること)
                    indices[idx++] = tl;
                    indices[idx++] = bl;
                    indices[idx++] = tr;

                    // 三角形 2 : tr, bl, br
                    indices[idx++] = tr;
                    indices[idx++] = bl;
                    indices[idx++] = br;
                }
            }

            // 法線とタンジェント計算（invertNormals フラグで反転可能）
            // シェーダやレンダラで法線が反転して見える場合、invertNormals=true にして試してください。
            ComputeNormalsAndTangents(vertices, indices, /*invertNormals*/ false);

            // バッファ作成（エラー処理を追加する）
            HRESULT hr = S_OK;
            D3D11_BUFFER_DESC bd{};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bd.ByteWidth = static_cast<UINT>(sizeof(VERTEX_3D) * vertexCount);

            D3D11_SUBRESOURCE_DATA sd{};
            sd.pSysMem = vertices.data();

            hr = m_graphicContext->GetDevice()->CreateBuffer(&bd, &sd, comp->meshRenderer->mesh.m_VertexBuffer.GetAddressOf());
            if (FAILED(hr)) {
				// エラーハンドリング
                return;
            }

            bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
            bd.ByteWidth = static_cast<UINT>(sizeof(unsigned int) * indexCount);
            sd.pSysMem = indices.data();

            hr = m_graphicContext->GetDevice()->CreateBuffer(&bd, &sd, comp->meshRenderer->mesh.m_IndexBuffer.GetAddressOf());
            if (FAILED(hr)) {
                // エラーハンドリング
                comp->meshRenderer->mesh.m_VertexBuffer.Reset();
                return;
            }

            comp->meshRenderer->mesh.meshCount = vertexCount;
            comp->meshRenderer->mesh.indexCount = indexCount;
            comp->CurrentScale = comp->Scale;

            auto* col = context->component->GetComponent<ColliderComponent>(entity);
            if (col) col->needsUpdate = true;
        }
    }
};
