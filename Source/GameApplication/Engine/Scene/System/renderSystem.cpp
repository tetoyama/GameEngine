// Engine/Scene/System/renderSystem.cpp
#include "renderSystem.h"
#include "Entity/entityRegistry.h"
#include "Component/transformComponent.h"
#include "Component/meshRendererComponent.h"
#include "Engine/Graphics/mainRenderer.h"

void RenderSystem::Initialize(){

	VERTEX_3D vertex[POLYGON_VERTEX]{};

	vertex[0].Position = XMFLOAT3(POLYGON_SIZE * -0.5f, POLYGON_SIZE * -0.5f, 0.0f);
	vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

	vertex[1].Position = XMFLOAT3(POLYGON_SIZE * 0.5f, POLYGON_SIZE * -0.5f, 0.0f);
	vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

	vertex[2].Position = XMFLOAT3(POLYGON_SIZE * -0.5f, POLYGON_SIZE * 0.5f, 0.0f);
	vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

	vertex[3].Position = XMFLOAT3(POLYGON_SIZE * 0.5f, POLYGON_SIZE * 0.5f, 0.0f);
	vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VERTEX_3D) * POLYGON_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	m_renderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);
	m_renderer->GetGraphicsContext()->CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
	m_renderer->GetGraphicsContext()->CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");
}

void RenderSystem::Draw(){
	const auto& meshComponents = m_registry->GetComponents();
	for(const auto& typePair : meshComponents){
		// MeshRendererComponent型のみ処理
		if(typePair.first == typeid(MeshRendererComponent)){
			for(const auto& entityPair : typePair.second){

				EntityID entity = entityPair.first;
				auto* meshRenderer = static_cast<MeshRendererComponent*>(entityPair.second.get());

				if (!meshRenderer || !meshRenderer->mesh) {
					continue;
				}
				// TransformComponent取得（なければスキップ）
				auto* transform = m_registry->GetComponent<TransformComponent>(entity);
				if (!transform) {
					continue;
				}
				// MainRendererに描画を依頼



			}
		}
	}
}

void RenderSystem::Finalize(){


}
