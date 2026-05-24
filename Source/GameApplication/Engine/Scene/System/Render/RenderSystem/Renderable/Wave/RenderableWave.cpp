// =======================================================================
// 
// RenderableWave.cpp
// 
// =======================================================================
#include "RenderableWave.h"

#include <d3d11.h>
#include "../../RenderPass/RenderPassContext.h"

#include "DebugTools/DebugSystem.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/componentRegistry.h"

#include "Scene/Component/waveComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/textureComponent.h"
#include <Component/materialComponent.h>
#include "Shader/commonDefine.h"


void RenderableWave::Execute(const RenderPassContext& ctx, SceneContext* sceneContext, const Entity& entity) {
	auto m_ComponentRegistry= pSceneContext->pComponent;
	auto m_PTransform= componentRegistry->GetComponent<TransformComponent>(pEntity);
	auto m_PWave= componentRegistry->GetComponent<WaveComponent>(pEntity);

	if (!pWave || !pWave->pMeshRenderer) {
		return;
	}
	GraphicsContext* graphicsContext = sceneContext->pManager->pGraphics;
	ID3D11Device* device = graphicsContext->GetDevice();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	TextureComponent* pTexture = sceneContext->pComponent->GetComponent<TextureComponent>(entity);
	MaterialComponent* pMaterial = sceneContext->pComponent->GetComponent<MaterialComponent>(entity);
	MATERIAL m_Material;
	if (pMaterial) {
		material = pMaterial->Material;
		material.MaterialFlags &= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
	}

	if(pTexture && pTexture->m_TextureData && pTexture->m_TextureData->pTexture){
		deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());

		// sceneContext->pManager->pGraphics->GetDeviceContext()->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
		material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;

	}

	sceneContext->pManager->pGraphics->SetMaterial(material);

	auto m_MeshRenderer= pWave->pMeshRenderer;
	auto m_Transform= pTransform;


	DirectX::XMMATRIX m_World= transform->CalculateWorldMatrix(transform, componentRegistry);

	graphicsContext->SetCullMode(CullMode::Back);
	graphicsContext->SetWorldMatrix(World);
	UINT m_Stride= sizeof(VERTEX_3D);
	UINT m_Offset= 0;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetIndexBuffer(*meshRenderer->mesh.m_IndexBuffer.GetAddressOf(), DXGI_FORMAT_R32_UINT, 0);

	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0, 0);

	//graphicsContext->SetDepthMode(DepthMode::Write);
	//graphicsContext->SetViewMatrix(ctx.viewMatrix);
	//graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}
