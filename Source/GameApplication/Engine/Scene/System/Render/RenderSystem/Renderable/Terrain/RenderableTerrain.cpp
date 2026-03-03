// =======================================================================
// 
// RenderableTerrain.cpp
// 
// =======================================================================
#include "RenderableTerrain.h"

#include <d3d11.h>
#include "../../RenderPass/RenderPassContext.h"

#include "DebugTools/DebugSystem.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/componentRegistry.h"

#include "Scene/Component/terrainComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/textureComponent.h"
#include <Component/materialComponent.h>

void RenderableTerrain::Execute(const RenderPassContext& ctx, SceneContext* sceneContext, const Entity& entity){

	TerrainComponent* pTerrain = sceneContext->component->GetComponent<TerrainComponent>(entity);
	TransformComponent* pTransform = sceneContext->component->GetComponent<TransformComponent>(entity);
	if(!pTerrain || !pTerrain->meshRenderer || !pTransform){
		return;
	}

	auto meshRenderer = pTerrain->meshRenderer;
	auto transform = pTransform;

	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	MATERIAL material{};
	MaterialComponent* pMaterial = sceneContext->component->GetComponent<MaterialComponent>(entity);
	if (pMaterial) {
		material = pMaterial->Material;
	}

	TextureComponent* pTexture = sceneContext->component->GetComponent<TextureComponent>(entity);
	if (pTexture) {

			// マテリアル設定
		if (pTexture->m_TextureData) {
			material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
			deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
		}

		graphicsContext->SetMaterial(material);

		UVMatrixBuffer uv;
		if (pTexture->UV_Slice_X != 0 && pTexture->UV_Slice_Y != 0) {
			uv.UVStart.x = (float)(pTexture->AnimationNum % pTexture->UV_Slice_X) * 1.0f / (float)pTexture->UV_Slice_X;
			uv.UVStart.y = (float)(pTexture->AnimationNum / pTexture->UV_Slice_X) * 1.0f / (float)pTexture->UV_Slice_Y;

			uv.UVEnd.x = (float)uv.UVStart.x + 1.0f / (float)pTexture->UV_Slice_X;
			uv.UVEnd.y = (float)uv.UVStart.y + 1.0f / (float)pTexture->UV_Slice_Y;
		}
		graphicsContext->SetUVMatrixBuffer(uv);

	} else {
		// マテリアル設定
		MATERIAL material{};
		material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		graphicsContext->SetMaterial(material);

		UVMatrixBuffer uv;
		graphicsContext->SetUVMatrixBuffer(uv);

	}

	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, sceneContext->component);

	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetIndexBuffer(*meshRenderer->mesh.m_IndexBuffer.GetAddressOf(), DXGI_FORMAT_R32_UINT, 0);

	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0, 0);

	graphicsContext->SetDepthMode(DepthMode::Write);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}
