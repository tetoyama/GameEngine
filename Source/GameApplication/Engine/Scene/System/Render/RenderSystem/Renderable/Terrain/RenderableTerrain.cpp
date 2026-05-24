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

	TerrainComponent* pTerrain = sceneContext->pComponent->GetComponent<TerrainComponent>(entity);
	TransformComponent* pTransform = sceneContext->pComponent->GetComponent<TransformComponent>(entity);
	if(!pTerrain || !pTerrain->pMeshRenderer || !pTransform){
		return;
	}

	auto m_MeshRenderer= pTerrain->pMeshRenderer;
	auto m_Transform= pTransform;

	GraphicsContext* graphicsContext = sceneContext->pManager->pGraphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	MATERIAL m_Material{};
	MaterialComponent* pMaterial = sceneContext->pComponent->GetComponent<MaterialComponent>(entity);
	if (pMaterial) {
		material = pMaterial->Material;
	}

	TextureComponent* pTexture = sceneContext->pComponent->GetComponent<TextureComponent>(entity);
	if (pTexture) {

			// マテリアル設定
		if (pTexture->m_TextureData) {
			material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
			deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
		}

		graphicsContext->SetMaterial(material);

		UVMatrixBuffer m_Uv;
		if (pTexture->UV_Slice_X > 0.0f && pTexture->UV_Slice_Y > 0.0f) {
			// UV_Slice_X/Y は「1セルのUVサイズ」
			// 例:
			// 0.25f = 4分割
			// 0.125f = 8分割

			int m_Column= (int)(1.0f / pTexture->UV_Slice_X);

			uv.UVStart.x = (pTexture->AnimationNum % column) * pTexture->UV_Slice_X;
			uv.UVStart.y = (pTexture->AnimationNum / column) * pTexture->UV_Slice_Y;

			// 1 セルの UV サイズ: 1/スライス数
			uv.UVEnd.x = uv.UVStart.x + 1.0f / pTexture->UV_Slice_X;  // セルの右端 UV
			uv.UVEnd.y = uv.UVStart.y + 1.0f / pTexture->UV_Slice_Y;  // セルの下端 UV
		}
		graphicsContext->SetUVMatrixBuffer(uv);

	} else {
		// マテリアル設定
		MATERIAL m_Material{};
		material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		graphicsContext->SetMaterial(material);

		UVMatrixBuffer m_Uv;
		graphicsContext->SetUVMatrixBuffer(uv);

	}

	DirectX::XMMATRIX m_World= transform->CalculateWorldMatrix(transform, pSceneContext->pComponent);

	graphicsContext->SetWorldMatrix(World);
	UINT m_Stride= sizeof(VERTEX_3D);
	UINT m_Offset= 0;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetIndexBuffer(*meshRenderer->mesh.m_IndexBuffer.GetAddressOf(), DXGI_FORMAT_R32_UINT, 0);

	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0, 0);

	graphicsContext->SetDepthMode(DepthMode::Write);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}
