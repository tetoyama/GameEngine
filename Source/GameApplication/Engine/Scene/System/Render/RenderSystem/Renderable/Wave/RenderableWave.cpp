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
	auto componentRegistry = sceneContext->component;
	auto pTransform = componentRegistry->GetComponent<TransformComponent>(entity);
	auto pWave = componentRegistry->GetComponent<WaveComponent>(entity);

	if (!pWave || !pWave->meshRenderer) {
		return;
	}
	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11Device* device = graphicsContext->GetDevice();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	TextureComponent* pTexture = sceneContext->component->GetComponent<TextureComponent>(entity);
	MaterialComponent* pMaterial = sceneContext->component->GetComponent<MaterialComponent>(entity);
	MATERIAL material;
	if (pMaterial) {
		material = pMaterial->Material;
		material.MaterialFlags &= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
	}

	if(pTexture && pTexture->m_TextureData && pTexture->m_TextureData->pTexture){
		deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());

		// sceneContext->manager->graphics->GetDeviceContext()->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
		material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;

	}

	sceneContext->manager->graphics->SetMaterial(material);

	auto meshRenderer = pWave->meshRenderer;
	auto transform = pTransform;


	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, componentRegistry);

	graphicsContext->SetCullMode(CullMode::BACK);
	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetIndexBuffer(*meshRenderer->mesh.m_IndexBuffer.GetAddressOf(), DXGI_FORMAT_R32_UINT, 0);

	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0, 0);

	//graphicsContext->SetDepthMode(DepthMode::WRITE);
	//graphicsContext->SetViewMatrix(ctx.viewMatrix);
	//graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}
