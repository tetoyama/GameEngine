#include "RenderableMesh.h"

#include <d3d11.h>
#include "../RenderableContext.h"

#include "GameApplication/Engine/DebugTools/DebugSystem.h"

#include "GameApplication/Engine/Scene/scene.h"
#include "GameApplication/Engine/Scene/sceneManager.h"
#include "GameApplication/Engine/Scene/Registry/componentRegistry.h"

#include "GameApplication/Engine/Scene/Component/meshRendererComponent.h"
#include "GameApplication/Engine/Scene/Component/transformComponent.h"
#include "GameApplication/Engine/Scene/Component/textureComponent.h"

void RenderableMesh::Execute(const RenderableContext& ctx, SceneContext* sceneContext, const Entity& entity){

	MeshRendererComponent* meshRenderer = sceneContext->component->GetComponent<MeshRendererComponent>(entity);
	TransformComponent* transform = sceneContext->component->GetComponent<TransformComponent>(entity);
	if(!meshRenderer || !transform){
		return;
	}
	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11Device* device = graphicsContext->GetDevice();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	ComponentRegistry* componentRegistry = sceneContext->component;

	TextureComponent* pTexture = sceneContext->component->GetComponent<TextureComponent>(entity);

	if(!pTexture){
		if(meshRenderer->mesh.m_TextureData){
			deviceContext->PSSetShaderResources(0, 1, meshRenderer->mesh.m_TextureData->pTexture.GetAddressOf());

			MATERIAL material{};
			material.DiffuseTextureEnable = true;

			material.Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
			graphicsContext->SetMaterial(material);
		}
	}
	if(meshRenderer->mesh.m_VertexLayout){
		deviceContext->IASetInputLayout(meshRenderer->mesh.m_VertexLayout.Get());
	}
	if(meshRenderer->mesh.m_VertexShader){
		deviceContext->VSSetShader(meshRenderer->mesh.m_VertexShader.Get(), NULL, 0);
	}
	if(meshRenderer->mesh.m_PixelShader){
		deviceContext->PSSetShader(meshRenderer->mesh.m_PixelShader.Get(), NULL, 0);
	}
	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, componentRegistry);

	graphicsContext->SetWorldViewProjection2D();
	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	if (ctx.passPhase == RenderPhase::PHASE_SHADOW) {
		deviceContext->PSSetShader(nullptr, NULL, 0); // ピクセルシェーダー無効化
	}
	deviceContext->Draw(meshRenderer->mesh.meshCount, 0);

	graphicsContext->SetDepthEnable(true);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}
