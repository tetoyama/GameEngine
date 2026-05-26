// =======================================================================
// 
// RenderableMesh.cpp
// 
// =======================================================================
#include "RenderableMesh.h"

#include <d3d11.h>
#include "../../RenderPass/RenderPassContext.h"

#include "DebugTools/DebugSystem.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/componentRegistry.h"

#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/textureComponent.h"

void RenderableMesh::Execute(const RenderPassContext& ctx, SceneContext* sceneContext, const Entity& entity){

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
		if(meshRenderer->mesh.textureData){
			deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, meshRenderer->mesh.textureData->pTexture.GetAddressOf());

			MATERIAL material{};
			material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;

			material.BaseColor = DirectX::XMFLOAT4(1, 1, 1, 1);
			graphicsContext->SetMaterial(material);
		}
	}
	//if(meshRenderer->mesh.m_VertexLayout){
	//	deviceContext->IASetInputLayout(meshRenderer->mesh.m_VertexLayout.Get());
	//}
	//if(meshRenderer->mesh.vertexShader){
	//	deviceContext->VSSetShader(meshRenderer->mesh.vertexShader.Get(), NULL, 0);
	//}
	//if(meshRenderer->mesh.pixelShader){
	//	deviceContext->PSSetShader(meshRenderer->mesh.pixelShader.Get(), NULL, 0);
	//}
	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, componentRegistry);

	graphicsContext->SetWorldViewProjection2D();
	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.vertexBuffer.GetAddressOf(), &stride, &offset);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//if (ctx.passPhase == RenderPhase::PHASE_SHADOW) {
	//	deviceContext->PSSetShader(nullptr, NULL, 0); // ピクセルシェーダー無効化
	//}
	deviceContext->Draw(meshRenderer->mesh.meshCount, 0);

	graphicsContext->SetDepthMode(DepthMode::WRITE);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}
