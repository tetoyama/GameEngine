// =======================================================================
// 
// RenderableModel.cpp
// 
// =======================================================================
#include "RenderableModel.h"

#include <d3d11.h>
#include "../../RenderPass/RenderPassContext.h"

#include "DebugTools/DebugSystem.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/componentRegistry.h"

#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/textureComponent.h"

#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"
#include "Backends/Assimp/matrix4x4.h"
#include <Component/materialComponent.h>

void RenderableModel::Execute(const RenderPassContext& ctx, SceneContext* sceneContext, const Entity& entity) {

	ModelRendererComponent* modelRenderer = sceneContext->pComponent->GetComponent<ModelRendererComponent>(entity);
	TransformComponent* transform = sceneContext->pComponent->GetComponent<TransformComponent>(entity);
	if (!modelRenderer || !transform) {
		return;
	}
	ModelData* pModel = modelRenderer->model.get();
	if (!pModel || !pModel->AiScene) {
		//sceneContext->pManager->pDebug->LOG_ERROR("ModelData is null or AiScene is not initialized.");
		return;
	}
	GraphicsContext* graphicsContext = sceneContext->pManager->pGraphics;
	ID3D11Device* device = graphicsContext->GetDevice();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	TextureComponent* pTexture = sceneContext->pComponent->GetComponent<TextureComponent>(entity);
	MaterialComponent* pMaterial = sceneContext->pComponent->GetComponent<MaterialComponent>(entity);
	MATERIAL m_Material;
	material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	if (pMaterial) {
		material = pMaterial->Material;
		// Preserve user-defined flags (env map), clear auto-computed texture flags
		material.MaterialFlags &= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
	}

	if (pTexture) {

			// マテリアル設定
		if (pTexture->m_TextureData) {
			material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
			deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
		}

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
		UVMatrixBuffer m_Uv;
		uv.UVStart = float2(0, 0);
		uv.UVEnd = float2(1, 1);
		graphicsContext->SetUVMatrixBuffer(uv);
	}
	graphicsContext->SetMaterial(material);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DirectX::XMMATRIX m_World= transform->CalculateWorldMatrix(transform, pSceneContext->pComponent);

	graphicsContext->SetCullMode(CullMode::Back);

	for (unsigned int m = 0; m < pModel->AiScene->mNumMeshes; m++) {

		if(!pTexture || !pTexture->m_TextureData){
			MATERIAL m_MaterialData= material;
			// Preserve user-defined flags (env map), clear auto-computed texture flags
			materialData.MaterialFlags &= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
			aiMaterial* aiMat = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
			aiColor4D m_Color;
			if(aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS){
				materialData.BaseColor = {color.r, color.g, color.b, color.a};

				if(pMaterial){
					materialData.BaseColor.x *= pMaterial->Material.BaseColor.x;
					materialData.BaseColor.y *= pMaterial->Material.BaseColor.y;
					materialData.BaseColor.z *= pMaterial->Material.BaseColor.z;
					materialData.BaseColor.w *= pMaterial->Material.BaseColor.w;
				}
			}

			aiMaterial* aimaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
			aiString m_TexName;
			if(aimaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texName) == AI_SUCCESS && texName.length > 0){
				auto m_It= pModel->m_Texture.find(texName.C_Str());
				if(it != pModel->m_Texture.end()){
					deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, &it->second);
					materialData.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
				}
			}
			if(aimaterial->GetTexture(aiTextureType_NORMALS, 0, &texName) == AI_SUCCESS && texName.length > 0){
				auto m_It= pModel->m_Texture.find(texName.C_Str());
				if(it != pModel->m_Texture.end()){
					deviceContext->PSSetShaderResources(1, 1, &it->second);
					materialData.MaterialFlags |= MATERIAL_FLAG_USE_NORMAL_TEXTURE;
				}
			}
			graphicsContext->SetMaterial(materialData);
		} else{

			deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
			if(pModel->SetTexture){
				material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
			}
			graphicsContext->SetMaterial(material);
		}
		
		graphicsContext->SetWorldMatrix(World);

		// 頂点バッファ設定
		UINT m_Stride= sizeof(VERTEX_3D);
		UINT m_Offset= 0;
		if (modelRenderer->blendedAnimations.size() > 0) {
			graphicsContext->GetDeviceContext()->IASetVertexBuffers(0, 1, &modelRenderer->dynamicVertexBuffers[m], &stride, &offset);
		} else {
			graphicsContext->GetDeviceContext()->IASetVertexBuffers(0, 1, &pModel->VertexBuffer[m], &stride, &offset);
		}
		// インデックスバッファ設定
		graphicsContext->GetDeviceContext()->IASetIndexBuffer(pModel->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		deviceContext->DrawIndexed(pModel->AiScene->mMeshes[m]->mNumFaces * 3, 0, 0);
	}
}
