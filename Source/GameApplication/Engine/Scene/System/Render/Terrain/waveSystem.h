#pragma once
#include "Interface/ISystem.h"
#include "Graphics/GraphicsContext.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/registry/ComponentRegistry.h"
#include "Component/meshRendererComponent.h"
#include "Component/waveComponent.h"

#include <cmath>
#include <algorithm>

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

	void Update(float dt) override{
		UpdateAllWaves(dt);
	}

	void EditorUpdate(float dt) override{
		UpdateAllWaves(dt);
	}

private:
	SceneManagerContext* m_context = nullptr;
	GraphicsContext* m_graphicContext = nullptr;

	// ============================================================
	// Gerstner Wave パラメータ
	// ============================================================
	static constexpr int WAVE_COUNT = 4;

	struct GerstnerWave {
		float dirX, dirZ;   // 正規化された波の進行方向
		float amplitude;     // 振幅（基準の倍率）
		float wavelength;    // 波長（基準の倍率）
		float speed;         // 速度（基準の倍率）
	};

	// 4つのオクターブ — 異なる方向・スケールを重ね合わせて自然な波面を作る
	static constexpr GerstnerWave kWaves[WAVE_COUNT] = {
		{  0.7f,  0.7f,   1.00f, 1.00f, 1.0f },   // メイン波（対角方向）
		{ -0.4f,  0.9f,   0.50f, 0.60f, 1.3f },   // サブ波（少しずれた方向）
		{  0.9f, -0.3f,   0.25f, 0.35f, 0.8f },   // 細波（横方向）
		{ -0.6f, -0.8f,   0.12f, 0.22f, 1.6f },   // さざ波（逆方向）
	};

	// ============================================================
	void UpdateAllWaves(float dt){
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
				vertices[idx].Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
				vertices[idx].TexCoord = DirectX::XMFLOAT2((float)x / grid, (float)z / grid);
				vertices[idx].Diffuse = DirectX::XMFLOAT4(0.3f, 0.6f, 1.0f, 1.0f);
			}
		}

		int idx = 0;
		for(int z = 0; z < grid; ++z){
			for(int x = 0; x < grid; ++x){
				int topLeft = z * (grid + 1) + x;
				int topRight = topLeft + 1;
				int bottomLeft = (z + 1) * (grid + 1) + x;
				int bottomRight = bottomLeft + 1;
				indices[idx++] = topLeft;
				indices[idx++] = bottomLeft;
				indices[idx++] = topRight;
				indices[idx++] = topRight;
				indices[idx++] = bottomLeft;
				indices[idx++] = bottomRight;
			}
		}

		comp->meshRenderer->mesh.meshCount = vertexCount;
		comp->meshRenderer->mesh.indexCount = indexCount;

		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(VERTEX_3D) * vertexCount;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = vertices.data();
		m_graphicContext->GetDevice()->CreateBuffer(&bd, &sd, comp->meshRenderer->mesh.m_VertexBuffer.GetAddressOf());

		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(unsigned int) * indexCount;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		sd.pSysMem = indices.data();
		m_graphicContext->GetDevice()->CreateBuffer(&bd, &sd, comp->meshRenderer->mesh.m_IndexBuffer.GetAddressOf());

		comp->CurrentResolution = comp->Resolution;
	}

	// ============================================================
	// Gerstner Wave 頂点更新
	//
	// 各頂点に対して WAVE_COUNT 個の波を合成する。
	// Gerstner 波は水平方向にも頂点を移動させるため、
	// 通常の正弦波よりリアルな鋭い波頭と平らな谷を再現する。
	// ============================================================
	void UpdateWaveVertices(WaveComponent* comp, float dt){
		if(!comp->meshRenderer) return;
		ID3D11Buffer* vbuf = comp->meshRenderer->mesh.m_VertexBuffer.Get();
		if(!vbuf) return;

		int grid = comp->Resolution;
		int vertexCount = (grid + 1) * (grid + 1);

		static std::vector<VERTEX_3D> tempVertices;
		tempVertices.resize(vertexCount);

		const float baseA = comp->Amplitude;
		const float baseLambda = std::max(comp->Wavelength, 0.01f);
		const float baseSpeed = comp->Speed;
		const float Q = std::clamp(comp->Steepness, 0.0f, 1.0f);
		const float t = comp->Time;

		for(int z = 0; z <= grid; ++z){
			for(int x = 0; x <= grid; ++x){
				int idx = z * (grid + 1) + x;
				float px0 = ((float)x / grid - 0.5f) * 2.0f;
				float pz0 = ((float)z / grid - 0.5f) * 2.0f;

				// Gerstner 波の合成
				float sumX = 0.0f;
				float sumY = 0.0f;
				float sumZ = 0.0f;

				// 法線用の偏微分蓄積
				float sumNx = 0.0f;
				float sumNy = 0.0f;
				float sumNz = 0.0f;

				for(int w = 0; w < WAVE_COUNT; ++w){
					const auto& wave = kWaves[w];

					// 方向を正規化
					float len = sqrtf(wave.dirX * wave.dirX + wave.dirZ * wave.dirZ);
					float dx = (len > 0.001f) ? wave.dirX / len : 1.0f;
					float dz = (len > 0.001f) ? wave.dirZ / len : 0.0f;

					float A   = baseA * wave.amplitude;
					float lam = baseLambda * wave.wavelength;
					float k   = 2.0f * DirectX::XM_PI / lam;
					float spd = baseSpeed * wave.speed;
					float omega = sqrtf(9.81f * k) * spd;   // 分散関係 ω = √(gk)

					// dot(D, P0)
					float dotDP = dx * px0 + dz * pz0;
					float phase = k * dotDP - omega * t;

					float sinP = sinf(phase);
					float cosP = cosf(phase);

					// Gerstner 変位: Q が 0 なら純粋な正弦波、1 に近づくほど鋭い波頭
					float Qi = Q / (k * baseA * WAVE_COUNT);  // 波ごとに正規化して自己交差を防ぐ
					Qi = std::min(Qi, 1.0f / (k * A * WAVE_COUNT + 0.001f));

					sumX += Qi * A * dx * cosP;
					sumZ += Qi * A * dz * cosP;
					sumY += A * sinP;

					// 法線蓄積 (Gerstner 解析導関数)
					float wA = k * A;
					sumNx += dx * wA * cosP;
					sumNz += dz * wA * cosP;
					sumNy += Qi * wA * sinP;
				}

				float finalX = px0 - sumX;
				float finalY = sumY;
				float finalZ = pz0 - sumZ;

				// 法線 (Gerstner の解析的法線)
				float nx = -sumNx;
				float ny = 1.0f - sumNy;
				float nz = -sumNz;
				float nLen = sqrtf(nx * nx + ny * ny + nz * nz);
				if(nLen > 0.001f){
					nx /= nLen;
					ny /= nLen;
					nz /= nLen;
				} else {
					nx = 0.0f; ny = 1.0f; nz = 0.0f;
				}

				tempVertices[idx].Position = DirectX::XMFLOAT3(finalX, finalY, finalZ);
				tempVertices[idx].Normal = DirectX::XMFLOAT3(nx, ny, nz);
				tempVertices[idx].TexCoord = DirectX::XMFLOAT2((float)x / grid, (float)z / grid);
				tempVertices[idx].Diffuse = DirectX::XMFLOAT4(0.3f, 0.6f, 1.0f, 1.0f);
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
