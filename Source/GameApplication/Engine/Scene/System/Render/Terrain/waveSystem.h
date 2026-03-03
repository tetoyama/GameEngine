#pragma once
#include "Interface/ISystem.h"
#include "Graphics/GraphicsContext.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/registry/ComponentRegistry.h"
#include "Component/meshRendererComponent.h"
#include "Component/waveComponent.h"

// ============================================================
// Gerstner 波パラメータ (内部用)
// ============================================================
struct GerstnerWaveParam {
	float dx, dz;   // 波の進行方向 (正規化)
	float A;        // 振幅
	float k;        // 波数  = 2π / 波長
	float omega;    // 角周波数 = 2π / 周期
	float Q;        // スティープネス (0 = サイン波, 1 = 最大傾斜)
};

class WaveSystem: public ISystem {
public:

	const char* GetSystemName() const override{
		return "WaveSystem";
	}

	WaveSystem(SceneManagerContext* context)
		: m_context(context){}

	~WaveSystem(){}

	void Initialize() override{
		m_graphicContext = m_context->graphics;

		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<WaveComponent>();
			for (auto entity : entities) {
				CreateMesh(context,entity);
			}
		}
	}

	void Finalize() override{

		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<WaveComponent>();
			for (auto entity : entities) {
				auto* comp = context->component->GetComponent<WaveComponent>(entity);
				if (comp && comp->meshRenderer) {
					comp->meshRenderer->mesh.m_IndexBuffer.Reset();
					comp->meshRenderer->mesh.m_VertexBuffer.Reset();
					delete comp->meshRenderer;
					comp->meshRenderer = nullptr;
				}
			}
		}
	}

	void EditorUpdate(float dt) override{
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<WaveComponent>();
			for (auto entity : entities) {
				auto* comp = context->component->GetComponent<WaveComponent>(entity);
				if (!comp) continue;

				if (!comp->meshRenderer || comp->Resolution != comp->CurrentResolution) {
					CreateMesh(context,entity);
				}
				UpdateWaveVertices(comp, dt);
			}
		}
	}

private:
	SceneManagerContext* m_context = nullptr;
	GraphicsContext* m_graphicContext = nullptr;

	// ============================================================
	// Gerstner 波パラメータ生成
	// comp の Amplitude / Wavelength / Speed / Steepness を基準に
	// 3方向の波を定義する。
	// ============================================================
	void BuildGerstnerWaves(const WaveComponent* comp, GerstnerWaveParam out[3]) const {
		const float twoPi = 2.0f * DirectX::XM_PI;

		// 波の振幅を 3 波で配分 (合計が comp->Amplitude に近くなるように)
		const float ampScale[3] = { 1.0f, 0.6f, 0.3f };
		// 波長倍率 (基本波長に対する倍率)
		const float wlScale[3]  = { 1.0f, 0.7f, 0.45f };
		// 進行方向 (2D, XZ 平面)
		const float dirs[3][2]  = {
			{  1.0f,  0.0f },
			{  0.7f,  0.7f },
			{ -0.4f,  0.9f }
		};

		const float wavePeriod = 1.0f; // 波の周期 (秒)
		const int   waveCount  = 3;    // 組み合わせる波の数
		float steepnessQ = saturateF(comp->Steepness);

		for (int i = 0; i < waveCount; ++i) {
			float wl    = comp->Wavelength * wlScale[i];
			GerstnerWaveParam& p = out[i];
			p.dx    = dirs[i][0];
			p.dz    = dirs[i][1];
			p.A     = comp->Amplitude * ampScale[i];
			p.k     = twoPi / wl;
			p.omega = twoPi / wavePeriod * comp->Speed;
			// 頂点が折り重ならないよう各波の Q を正規化 (waveCount 波合計で Steepness を分配)
			p.Q     = steepnessQ / (p.k * p.A * float(waveCount) + 1e-5f);
		}
	}

	static float saturateF(float v){ return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

	void CreateMesh(SceneContext* context, Entity entity){

		auto* comp = context->component->GetComponent<WaveComponent>(entity);
		if(!comp) return;

		if(comp->meshRenderer){
			comp->meshRenderer->mesh.m_IndexBuffer.Reset();
			comp->meshRenderer->mesh.m_VertexBuffer.Reset();
		} else{
			comp->meshRenderer = new MeshRendererComponent();
		}

		int grid = comp->Resolution;
		int vertexCount = (grid + 1) * (grid + 1);
		int indexCount = grid * grid * 6;
		std::vector<VERTEX_3D> vertices(vertexCount);
		std::vector<unsigned int> indices(indexCount);

		for(int z = 0; z <= grid; ++z){
			for(int x = 0; x <= grid; ++x){
				int idx = z * (grid + 1) + x;
				float px = ((float)x / grid - 0.5f) * 2.0f;
				float pz = ((float)z / grid - 0.5f) * 2.0f;

				vertices[idx].Position = DirectX::XMFLOAT3(px, 0.0f, pz);
				vertices[idx].Normal   = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
				vertices[idx].Tangent  = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
				vertices[idx].TexCoord = DirectX::XMFLOAT2((float)x / grid, (float)z / grid);
				vertices[idx].Diffuse  = DirectX::XMFLOAT4(0.3f, 0.6f, 1.0f, 1.0f);
			}
		}

		int idx = 0;
		for(int z = 0; z < grid; ++z){
			for(int x = 0; x < grid; ++x){
				int topLeft     = z * (grid + 1) + x;
				int topRight    = topLeft + 1;
				int bottomLeft  = (z + 1) * (grid + 1) + x;
				int bottomRight = bottomLeft + 1;
				indices[idx++] = topLeft;
				indices[idx++] = bottomLeft;
				indices[idx++] = topRight;
				indices[idx++] = topRight;
				indices[idx++] = bottomLeft;
				indices[idx++] = bottomRight;
			}
		}

		comp->meshRenderer->mesh.meshCount  = vertexCount;
		comp->meshRenderer->mesh.indexCount = indexCount;

		D3D11_BUFFER_DESC bd{};
		bd.Usage          = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth      = sizeof(VERTEX_3D) * vertexCount;
		bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = vertices.data();
		m_graphicContext->GetDevice()->CreateBuffer(&bd, &sd, comp->meshRenderer->mesh.m_VertexBuffer.GetAddressOf());

		bd.Usage          = D3D11_USAGE_DEFAULT;
		bd.ByteWidth      = sizeof(unsigned int) * indexCount;
		bd.BindFlags      = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		sd.pSysMem        = indices.data();
		m_graphicContext->GetDevice()->CreateBuffer(&bd, &sd, comp->meshRenderer->mesh.m_IndexBuffer.GetAddressOf());

		comp->CurrentResolution = comp->Resolution;
	}

	// ============================================================
	// Gerstner 波による頂点・法線・タンジェント更新
	// ============================================================
	void UpdateWaveVertices(WaveComponent* comp, float dt){
		if(!comp->meshRenderer) return;
		ID3D11Buffer* vbuf = comp->meshRenderer->mesh.m_VertexBuffer.Get();
		if(!vbuf) return;

		int grid        = comp->Resolution;
		int vertexCount = (grid + 1) * (grid + 1);

		static std::vector<VERTEX_3D> tempVertices;
		tempVertices.resize(vertexCount);

		// Gerstner 波パラメータを構築
		GerstnerWaveParam waves[3];
		BuildGerstnerWaves(comp, waves);

		for(int z = 0; z <= grid; ++z){
			for(int x = 0; x <= grid; ++x){
				int   idx = z * (grid + 1) + x;
				float px0 = ((float)x / grid - 0.5f) * 2.0f;
				float pz0 = ((float)z / grid - 0.5f) * 2.0f;

				// 最終位置 (Gerstner水平変位 + 垂直変位)
				float finalX = px0;
				float finalY = 0.0f;
				float finalZ = pz0;

				// 法線 (初期値: 上方向)
				float Nx = 0.0f, Ny = 1.0f, Nz = 0.0f;
				// タンジェント (初期値: X 方向)
				float Tx = 1.0f, Ty = 0.0f, Tz = 0.0f;

				for(int w = 0; w < 3; ++w){
					const GerstnerWaveParam& wp = waves[w];
					float phase = wp.k * (wp.dx * px0 + wp.dz * pz0) - wp.omega * comp->Time;
					float sinP  = sinf(phase);
					float cosP  = cosf(phase);

					// Gerstner 水平変位
					finalX += wp.Q * wp.A * wp.dx * cosP;
					finalZ += wp.Q * wp.A * wp.dz * cosP;
					// 垂直変位
					finalY += wp.A * sinP;

					// 法線更新 (解析的微分)
					Nx -= wp.dx * wp.k * wp.A * cosP;
					Nz -= wp.dz * wp.k * wp.A * cosP;
					Ny -= wp.Q  * wp.k * wp.A * sinP;

					// タンジェント更新 (X 方向の微分)
					Tx -= wp.Q  * wp.k * wp.A * wp.dx * wp.dx * sinP;
					Ty += wp.dx * wp.k * wp.A * cosP;
					Tz -= wp.Q  * wp.k * wp.A * wp.dx * wp.dz * sinP;
				}

				// 正規化
				float nLen = sqrtf(Nx*Nx + Ny*Ny + Nz*Nz);
				if(nLen > 1e-5f){ Nx /= nLen; Ny /= nLen; Nz /= nLen; }
				float tLen = sqrtf(Tx*Tx + Ty*Ty + Tz*Tz);
				if(tLen > 1e-5f){ Tx /= tLen; Ty /= tLen; Tz /= tLen; }

				tempVertices[idx].Position = DirectX::XMFLOAT3(finalX, finalY, finalZ);
				tempVertices[idx].Normal   = DirectX::XMFLOAT3(Nx, Ny, Nz);
				tempVertices[idx].Tangent  = DirectX::XMFLOAT3(Tx, Ty, Tz);
				tempVertices[idx].TexCoord = DirectX::XMFLOAT2((float)x / grid, (float)z / grid);
				tempVertices[idx].Diffuse  = DirectX::XMFLOAT4(0.3f, 0.6f, 1.0f, 1.0f);
			}
		}

		D3D11_MAPPED_SUBRESOURCE msr{};
		auto* ctx = m_context->graphics->GetDeviceContext();
		if(SUCCEEDED(ctx->Map(vbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))){
			memcpy(msr.pData, tempVertices.data(), sizeof(VERTEX_3D) * vertexCount);
			ctx->Unmap(vbuf, 0);
		}

		comp->Time += dt * comp->Speed;
	}
};
