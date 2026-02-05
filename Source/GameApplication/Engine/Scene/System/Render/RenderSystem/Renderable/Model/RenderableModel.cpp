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

	ModelRendererComponent* modelRenderer = sceneContext->component->GetComponent<ModelRendererComponent>(entity);
	TransformComponent* transform = sceneContext->component->GetComponent<TransformComponent>(entity);
	if (!modelRenderer || !transform) {
		return;
	}
	ModelData* pModel = modelRenderer->model.get();
	if (!pModel || !pModel->AiScene) {
		//sceneContext->manager->debug->LOG_ERROR("ModelData is null or AiScene is not initialized.");
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
		material.MaterialFlags = 0;
	}

	if (pTexture) {

			// マテリアル設定
		if (pTexture->m_TextureData) {
			material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
			deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
		}

		UVMatrix uv;
		if (pTexture->UV_Slice_X != 0 && pTexture->UV_Slice_Y != 0) {
			uv.Start.x = (float)(pTexture->AnimationNum % pTexture->UV_Slice_X) * 1.0f / (float)pTexture->UV_Slice_X;
			uv.Start.y = (float)(pTexture->AnimationNum / pTexture->UV_Slice_X) * 1.0f / (float)pTexture->UV_Slice_Y;

			uv.End.x = (float)uv.Start.x + 1.0f / (float)pTexture->UV_Slice_X;
			uv.End.y = (float)uv.Start.y + 1.0f / (float)pTexture->UV_Slice_Y;
		}
		graphicsContext->SetUVMatrix(uv);

	} else {
		UVMatrix uv;
		graphicsContext->SetUVMatrix(uv);
	}
	graphicsContext->SetMaterial(material);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, sceneContext->component);

	if (ctx.passPhase == RenderPhase::PHASE_SHADOW) {
		//deviceContext->PSSetShader(nullptr, NULL, 0); // ピクセルシェーダー無効化
	} else {

	}

	graphicsContext->SetCullMode(CullMode::Back);

	for (unsigned int m = 0; m < pModel->AiScene->mNumMeshes; m++) {

		if (pModel->SetTexture) {
			if(!pTexture){
				MATERIAL materialData;

				if (pMaterial) {
					materialData = pMaterial->Material;
				} else {
					materialData.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
				}

				material.MaterialFlags = 0;

				aiMaterial* aiMat = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
				aiColor4D color;
				if(aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS){
					materialData.BaseColor = {color.r, color.g, color.b, color.a};

					if(pMaterial){
						materialData.BaseColor.x *= pMaterial->Material.BaseColor.x;
						materialData.BaseColor.y *= pMaterial->Material.BaseColor.y;
						materialData.BaseColor.z *= pMaterial->Material.BaseColor.z;
						materialData.BaseColor.w *= pMaterial->Material.BaseColor.w;
					}
				}

				aiMaterial* material = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
				aiString texName;
				if(material->GetTexture(aiTextureType_DIFFUSE, 0, &texName) == AI_SUCCESS && texName.length > 0){
					auto it = pModel->m_Texture.find(texName.C_Str());
					if(it != pModel->m_Texture.end()){
						deviceContext->PSSetShaderResources(0, 1, &it->second);
						materialData.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
					}
				}
				if(material->GetTexture(aiTextureType_NORMALS, 0, &texName) == AI_SUCCESS && texName.length > 0){
					auto it = pModel->m_Texture.find(texName.C_Str());
					if(it != pModel->m_Texture.end()){
						deviceContext->PSSetShaderResources(1, 1, &it->second);
						materialData.MaterialFlags |= MATERIAL_FLAG_USE_NORMAL_TEXTURE;
					}
				}

				if(pModel->SetTexture){
				}
				graphicsContext->SetMaterial(materialData);
			} else{

				if(pTexture->m_TextureData){
					deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
					if(pModel->SetTexture){
						material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
					}
				}
				graphicsContext->SetMaterial(material);

			}
		}
		

		graphicsContext->SetWorldMatrix(World);

		// 頂点バッファ設定
		UINT stride = sizeof(VERTEX_3D);
		UINT offset = 0;
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
