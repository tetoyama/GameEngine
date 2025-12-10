#include "RenderableTerrain.h"

#include <d3d11.h>
#include "../../RenderPass/RenderPassContext.h"

#include "GameApplication/Engine/DebugTools/DebugSystem.h"

#include "GameApplication/Engine/Scene/scene.h"
#include "GameApplication/Engine/Scene/sceneManager.h"
#include "GameApplication/Engine/Scene/Registry/componentRegistry.h"

#include "GameApplication/Engine/Scene/Component/terrainComponent.h"
#include "GameApplication/Engine/Scene/Component/meshRendererComponent.h"
#include "GameApplication/Engine/Scene/Component/transformComponent.h"
#include "GameApplication/Engine/Scene/Component/textureComponent.h"

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

	TextureComponent* pTexture = sceneContext->component->GetComponent<TextureComponent>(entity);
	if (pTexture) {

			// マテリアル設定
		MATERIAL material = pTexture->Material;
		material.DiffuseTextureEnable = ((bool)pTexture->m_TextureData);
		if (pTexture->m_TextureData) {
			deviceContext->PSSetShaderResources(0, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
		}

		graphicsContext->SetMaterial(material);

		UVMatrix uv;
		if (pTexture->UV_Slice_X != 0 && pTexture->UV_Slice_Y != 0) {
			uv.Start.x = (float)(pTexture->AnimationNum % pTexture->UV_Slice_X) * 1.0f / (float)pTexture->UV_Slice_X;
			uv.Start.y = (float)(pTexture->AnimationNum / pTexture->UV_Slice_X) * 1.0f / (float)pTexture->UV_Slice_Y;

			uv.End.x = (float)uv.Start.x + 1.0f / (float)pTexture->UV_Slice_X;
			uv.End.y = (float)uv.Start.y + 1.0f / (float)pTexture->UV_Slice_Y;
		}
		graphicsContext->SetUVMatrix(uv);

	} else {
		// マテリアル設定
		MATERIAL material;
		material.DiffuseTextureEnable = false;
		material.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		graphicsContext->SetMaterial(material);

		UVMatrix uv;
		graphicsContext->SetUVMatrix(uv);

	}

	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, sceneContext->component);

	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetIndexBuffer(*meshRenderer->mesh.m_IndexBuffer.GetAddressOf(), DXGI_FORMAT_R32_UINT, 0);

	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0, 0);

	graphicsContext->SetDepthEnable(true);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}
