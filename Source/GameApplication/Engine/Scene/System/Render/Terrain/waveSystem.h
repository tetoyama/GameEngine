// =======================================================================
// 
// waveSystem.h
// 
// =======================================================================
#pragma once
#include "Interface/ISystem.h"
#include "Graphics/GraphicsContext.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/registry/ComponentRegistry.h"
#include "Component/meshRendererComponent.h"
#include "Component/waveComponent.h"

// 波メッシュの生成・更新を管理するシステム
class WaveSystem: public ISystem {
public:

	const char* GetSystemName() const override{
		return "WaveSystem";
	}

	WaveSystem(SceneManagerContext* context)
		: m_pContext(context){}

	~WaveSystem(){}

	void Initialize() override{
		m_pGraphicContext = m_pContext->graphics;

		for (auto& [name, scene] : m_pContext->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<WaveComponent>();
			for (auto entity : entities) {
				CreateMesh(context,entity);
			}
		}
	}

	void Finalize() override{

		for (auto& [name, scene] : m_pContext->sceneManager->GetActiveScenes()) {
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
		for (auto& [name, scene] : m_pContext->sceneManager->GetActiveScenes()) {
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
	SceneManagerContext* m_pContext = nullptr;
	GraphicsContext* m_pGraphicContext = nullptr;

	void CreateMesh(SceneContext* context, Entity entity){

		auto* comp = context->component->GetComponent<WaveComponent>(entity);
		if(!comp) return;

		if(comp->meshRenderer){
			comp->meshRenderer->mesh.m_IndexBuffer.Reset();
			comp->meshRenderer->mesh.m_VertexBuffer.Reset();
		} else{
			comp->meshRenderer = new MeshRendererComponent();
		}

		int m_Grid= comp->Resolution;
		int m_VertexCount= (grid + 1) * (grid + 1);
		int m_IndexCount= grid * grid * 6;
		std::vector<VERTEX_3D> vertices(vertexCount);
		std::vector<unsigned int> indices(indexCount);

		float m_Half= 1.0f;

		for(int z = 0; z <= grid; ++z){
			for(int x = 0; x <= grid; ++x){
				int m_Idx= z * (grid + 1) + x;
				float m_Px= ((float)x / grid - 0.5f) * 2.0f;
				float m_Pz= ((float)z / grid - 0.5f) * 2.0f;

				vertices[idx].Position = DirectX::XMFLOAT3(px, 0.0f, pz);
				vertices[idx].Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
				vertices[idx].TexCoord = DirectX::XMFLOAT2((float)x / grid, (float)z / grid);
				vertices[idx].Diffuse = DirectX::XMFLOAT4(0.3f, 0.6f, 1.0f, 1.0f);
			}
		}

		int m_Idx= 0;
		for(int z = 0; z < grid; ++z){
			for(int x = 0; x < grid; ++x){
				int m_TopLeft= z * (grid + 1) + x;
				int m_TopRight= topLeft + 1;
				int m_BottomLeft= (z + 1) * (grid + 1) + x;
				int m_BottomRight= bottomLeft + 1;
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

		D3D11_BUFFER_DESC m_Bd{};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(VERTEX_3D) * vertexCount;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		D3D11_SUBRESOURCE_DATA m_Sd{};
		sd.pSysMem = vertices.data();
		m_pGraphicContext->GetDevice()->CreateBuffer(&bd, &sd, comp->meshRenderer->mesh.m_VertexBuffer.GetAddressOf());

		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(unsigned int) * indexCount;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		sd.pSysMem = indices.data();
		m_pGraphicContext->GetDevice()->CreateBuffer(&bd, &sd, comp->meshRenderer->mesh.m_IndexBuffer.GetAddressOf());

		comp->CurrentResolution = comp->Resolution;
	}

	void UpdateWaveVertices(WaveComponent* comp){
		if(!comp->meshRenderer) return;
		ID3D11Buffer* vbuf = comp->meshRenderer->mesh.m_VertexBuffer.Get();
		if(!vbuf) return;

		int m_Grid= comp->Resolution;
		int m_VertexCount= (grid + 1) * (grid + 1);

		static std::vector<VERTEX_3D> m_TempVertices;
		tempVertices.resize(vertexCount);

		float m_Lambda= comp->Wavelength;
		float m_T= 1.0f;
		float m_A= comp->Amplitude;
		float m_Omega= 2.0f * DirectX::XM_PI / T;
		float m_K= 2.0f * DirectX::XM_PI / lambda;

		float m_Cx= 0.0f;
		float m_Cz= 0.0f;

		for(int z = 0; z <= grid; ++z){
			for(int x = 0; x <= grid; ++x){
				int m_Idx= z * (grid + 1) + x;
				float m_Px= ((float)x / grid - 0.5f) * 2.0f;
				float m_Pz= ((float)z / grid - 0.5f) * 2.0f;

				float m_Dx= px - cx;
				float m_Dz= pz - cz;
				float m_R= sqrtf(dx * dx + dz * dz);

				float m_Y= A * sinf(k * r - omega * comp->Time);

				tempVertices[idx].Position = DirectX::XMFLOAT3(px, y, pz);
				tempVertices[idx].Normal = DirectX::XMFLOAT3(0, 1, 0);
				tempVertices[idx].TexCoord = DirectX::XMFLOAT2((float)x / grid, (float)z / grid);
				tempVertices[idx].Diffuse = DirectX::XMFLOAT4(0.3f, 0.6f, 1.0f, 1.0f);
			}
		}

		D3D11_MAPPED_SUBRESOURCE m_Msr{};
		auto* ctx = m_pContext->graphics->GetDeviceContext();
		if(SUCCEEDED(ctx->Map(vbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))){
			memcpy(msr.pData, tempVertices.data(), sizeof(VERTEX_3D) * vertexCount);
			ctx->Unmap(vbuf, 0);
		}

		comp->Time += 0.02f * comp->Speed;
	}
};
