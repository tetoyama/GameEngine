#pragma once
#include "Interface/ISystem.h"
#include "Graphics/GraphicsContext.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/registry/ComponentRegistry.h"
#include "Component/meshRendererComponent.h"
#include "Component/waveComponent.h"

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
				UpdateWaveVertices(comp);
			}
		}
	}

private:
	SceneManagerContext* m_context = nullptr;
	GraphicsContext* m_graphicContext = nullptr;

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

		float half = 1.0f;

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

	void UpdateWaveVertices(WaveComponent* comp){
		if(!comp->meshRenderer) return;
		ID3D11Buffer* vbuf = comp->meshRenderer->mesh.m_VertexBuffer.Get();
		if(!vbuf) return;

		int grid = comp->Resolution;
		int vertexCount = (grid + 1) * (grid + 1);

		static std::vector<VERTEX_3D> tempVertices;
		tempVertices.resize(vertexCount);

		float lambda = comp->Wavelength;
		float T = 1.0f;
		float A = comp->Amplitude;
		float omega = 2.0f * DirectX::XM_PI / T;
		float k = 2.0f * DirectX::XM_PI / lambda;

		float cx = 0.0f;
		float cz = 0.0f;

		for(int z = 0; z <= grid; ++z){
			for(int x = 0; x <= grid; ++x){
				int idx = z * (grid + 1) + x;
				float px = ((float)x / grid - 0.5f) * 2.0f;
				float pz = ((float)z / grid - 0.5f) * 2.0f;

				float dx = px - cx;
				float dz = pz - cz;
				float r = sqrtf(dx * dx + dz * dz);

				float y = A * sinf(k * r - omega * comp->Time);

				// 法線計算 (波の方程式の偏微分)
				float nx = 0.0f;
				float nz = 0.0f;
				if(r > 0.001f){
					float cosArg = A * k * cosf(k * r - omega * comp->Time);
					nx = -cosArg * dx / r;
					nz = -cosArg * dz / r;
				}
				float ny = 1.0f;
				float nLen = sqrtf(nx * nx + ny * ny + nz * nz);
				nx /= nLen;
				ny /= nLen;
				nz /= nLen;

				tempVertices[idx].Position = DirectX::XMFLOAT3(px, y, pz);
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

		comp->Time += 0.02f * comp->Speed;
	}
};
