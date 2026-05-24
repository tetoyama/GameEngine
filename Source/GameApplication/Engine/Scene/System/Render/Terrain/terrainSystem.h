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
		: m_pContext(context) {}

	~TerrainSystem() {}

	void Initialize() override {
		m_pGraphicContext = m_pContext->graphics;
		for (auto& [name, scene] : m_pContext->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<TerrainComponent>();
			for (auto entity : entities) {

				CreateMesh(context,entity);
			}
		}
	}

	void Finalize() override {
		for (auto& [name, scene] : m_pContext->sceneManager->GetActiveScenes()) {
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
		for (auto& [name, scene] : m_pContext->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<TerrainComponent>();
			for (auto entity : entities) {

				CreateMesh(context,entity);
			}
		}
	}

private:
	SceneManagerContext* m_pContext = nullptr;
	GraphicsContext* m_pGraphicContext = nullptr;

    void ComputeNormalsAndTangents(std::vector<VERTEX_3D>& vertices, const std::vector<unsigned int>& indices, bool invertNormals = false) {
        // 法線をゼロ初期化（念のため）
        for (auto& v : vertices) {
            v.Normal = { 0.0f, 0.0f, 0.0f };
            v.Tangent = { 0.0f, 0.0f, 0.0f };
        }

        const size_t m_IndexCount= indices.size();
        for (size_t i = 0; i + 2 < indexCount; i += 3) {
            unsigned int m_I0= indices[i + 0];
            unsigned int m_I1= indices[i + 1];
            unsigned int m_I2= indices[i + 2];

            // 頂点読み出し
            DirectX::XMVECTOR m_P0= DirectX::XMLoadFloat3(&vertices[i0].Position);
            DirectX::XMVECTOR m_P1= DirectX::XMLoadFloat3(&vertices[i1].Position);
            DirectX::XMVECTOR m_P2= DirectX::XMLoadFloat3(&vertices[i2].Position);

            // 辺ベクトル（順序によって法線の向きが決まる）
            DirectX::XMVECTOR m_E1= DirectX::XMVectorSubtract(p1, p0);
            DirectX::XMVECTOR m_E2= DirectX::XMVectorSubtract(p2, p0);

            // 面法線（面積に比例する大きさ）
            DirectX::XMVECTOR m_FaceNormal= DirectX::XMVector3Cross(e1, e2);

            if (invertNormals) {
                faceNormal = DirectX::XMVectorNegate(faceNormal);
            }

            // faceNormal を各頂点に加算（面積重み付け）
            DirectX::XMFLOAT3 m_Fn;
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
            DirectX::XMFLOAT3 m_Tan;
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
            DirectX::XMVECTOR m_N= DirectX::XMLoadFloat3(&v.Normal);
            float m_Len= DirectX::XMVectorGetX(DirectX::XMVector3Length(n));
            if (len < 1e-6f) {
                // 異常な場合は上向きにフォールバック
                v.Normal = { 0.0f, 1.0f, 0.0f };
            } else {
                n = DirectX::XMVector3Normalize(n);
                DirectX::XMStoreFloat3(&v.Normal, n);
            }

            // タンジェント正規化（ゼロチェック）
            DirectX::XMVECTOR m_T= DirectX::XMLoadFloat3(&v.Tangent);
            float m_Tlen= DirectX::XMVectorGetX(DirectX::XMVector3Length(t));
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

            int m_GridSize= comp->Scale;
            int m_VertexCount= (gridSize + 1) * (gridSize + 1);
            int m_IndexCount= gridSize * gridSize * 6;

            std::vector<VERTEX_3D> vertices(vertexCount);
            std::vector<unsigned int> indices(indexCount);

            float m_HalfSize= gridSize * 0.5f;

            // 頂点生成
            for (int z = 0; z <= gridSize; ++z) {
                for (int x = 0; x <= gridSize; ++x) {
                    int m_I= z * (gridSize + 1) + x;
                    float m_Vx= (x - halfSize) / (float)gridSize;
                    float m_Vz= (z - halfSize) / (float)gridSize;
                    float m_Vy= 0.0f;
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
            int m_Idx= 0;
            for (int z = 0; z < gridSize; ++z) {
                for (int x = 0; x < gridSize; ++x) {
                    int m_Tl= z * (gridSize + 1) + x;
                    int m_Tr= tl + 1;
                    int m_Bl= (z + 1) * (gridSize + 1) + x;
                    int m_Br= bl + 1;

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
            HRESULT m_Hr= S_OK;
            D3D11_BUFFER_DESC m_Bd{};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bd.ByteWidth = static_cast<UINT>(sizeof(VERTEX_3D) * vertexCount);

            D3D11_SUBRESOURCE_DATA m_Sd{};
            sd.pSysMem = vertices.data();

            hr = m_pGraphicContext->GetDevice()->CreateBuffer(&bd, &sd, comp->meshRenderer->mesh.m_VertexBuffer.GetAddressOf());
            if (FAILED(hr)) {
				// エラーハンドリング
                return;
            }

            bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
            bd.ByteWidth = static_cast<UINT>(sizeof(unsigned int) * indexCount);
            sd.pSysMem = indices.data();

            hr = m_pGraphicContext->GetDevice()->CreateBuffer(&bd, &sd, comp->meshRenderer->mesh.m_IndexBuffer.GetAddressOf());
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
