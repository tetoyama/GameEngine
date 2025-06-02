// Engine/Scene/System/renderSystem.cpp
#include "renderSystem.h"
#include "Entity/entityRegistry.h"
#include "Component/transformComponent.h"
#include "Component/meshRendererComponent.h"
#include "Engine/Graphics/mainRenderer.h"
#include "DirectXMath.h"
#include "Engine/Resources/TextureLoader.h"

#define POLYGON_VERTEX (4)
#define POLYGON_SIZE (100)

void RenderSystem::Initialize(){
}

void RenderSystem::Draw(){
	m_renderer->GetGraphicsContext()->SetWorldViewProjection2D();

	const auto& meshComponents = m_registry->GetComponents();
	for(const auto& typePair : meshComponents){

		if(typePair.first == typeid(MeshRendererComponent)){
			for(const auto& entityPair : typePair.second){

				EntityID entity = entityPair.first;
				auto* meshRenderer = static_cast<MeshRendererComponent*>(entityPair.second.get());

				if(!meshRenderer || !meshRenderer->mesh){
					continue;
				}
				auto* transform = m_registry->GetComponent<TransformComponent>(entity);
				if(!transform){
					continue;
				}

				m_renderer->GetGraphicsContext()->GetDeviceContext()->IASetInputLayout(meshRenderer->mesh->m_VertexLayout);

				m_renderer->GetGraphicsContext()->GetDeviceContext()->VSSetShader(meshRenderer->mesh->m_VertexShader, NULL, 0);
				m_renderer->GetGraphicsContext()->GetDeviceContext()->PSSetShader(meshRenderer->mesh->m_PixelShader, NULL, 0);


				DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationRollPitchYaw(transform->rotation.x, transform->rotation.y, transform->rotation.z);
				DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(transform->scale.x, transform->scale.y, transform->scale.z);
				DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(transform->position.x, transform->position.y, transform->position.z);

				m_renderer->GetGraphicsContext()->SetWorldMatrix(Scale * Rotation * Translation);
				UINT stride = sizeof(VERTEX_3D);
				UINT offset = 0;

				m_renderer->GetGraphicsContext()->GetDeviceContext()->IASetVertexBuffers(0, 1, &meshRenderer->mesh->m_VertexBuffer, &stride, &offset);

				SetTexture(meshRenderer->mesh->m_TextureID);

				MATERIAL material;
				material.Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);

				m_renderer->GetGraphicsContext()->SetMaterial(material);

				m_renderer->GetGraphicsContext()->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

				if(meshRenderer->mesh->m_IndexBuffer){

					m_renderer->GetGraphicsContext()->GetDeviceContext()->IASetIndexBuffer(
						meshRenderer->mesh->m_IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

					m_renderer->GetGraphicsContext()->GetDeviceContext()->DrawIndexed(meshRenderer->mesh->indexCount, 0, 0);

				} else{
					m_renderer->GetGraphicsContext()->GetDeviceContext()->Draw(meshRenderer->mesh->meshCount, 0);
				}
			}
		}
	}
	m_renderer->GetGraphicsContext()->SetViewMatrix(DirectX::XMMatrixLookAtLH({0.0f,0.0f,0.0f}, {0.0,0.0f,1.0f}, {0.0f,1.0f,0.0f}));

}

void RenderSystem::Finalize(){

}
