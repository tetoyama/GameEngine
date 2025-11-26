#include "RenderableModel.h"

#include <d3d11.h>
#include "../RenderableContext.h"

#include "GameApplication/Engine/DebugTools/DebugSystem.h"

#include "GameApplication/Engine/Scene/scene.h"
#include "GameApplication/Engine/Scene/sceneManager.h"
#include "GameApplication/Engine/Scene/Registry/componentRegistry.h"

#include "GameApplication/Engine/Scene/Component/modelRendererComponent.h"
#include "GameApplication/Engine/Scene/Component/transformComponent.h"
#include "GameApplication/Engine/Scene/Component/textureComponent.h"

#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"
#include "Backends/Assimp/matrix4x4.h"

void RenderableModel::Execute(const RenderableContext& ctx, SceneContext* sceneContext, const Entity& entity){

	ModelRendererComponent* modelRenderer = sceneContext->component->GetComponent<ModelRendererComponent>(entity);
	TransformComponent* transform = sceneContext->component->GetComponent<TransformComponent>(entity);
	if(!modelRenderer || !transform){
		return;
	}
	ModelData* pModel = modelRenderer->model.get();
	if(!pModel || !pModel->AiScene){
		sceneContext->manager->debug->LOG_ERROR("ModelData is null or AiScene is not initialized.");
		return;
	}
	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11Device* device = graphicsContext->GetDevice();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	TextureComponent* pTexture = sceneContext->component->GetComponent<TextureComponent>(entity);



	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pModel->Update(modelRenderer->animationTime, sceneContext->manager->graphics);

	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, sceneContext->component);
	if(modelRenderer->pixelShader){
		deviceContext->PSSetShader(modelRenderer->pixelShader->m_PixelShader.Get(), nullptr, 0);
	}
	if(modelRenderer->vertexShader){
		deviceContext->IASetInputLayout(modelRenderer->vertexShader->m_VertexLayout.Get());
		deviceContext->VSSetShader(modelRenderer->vertexShader->m_VertexShader.Get(), nullptr, 0);
	}
	graphicsContext->SetCullMode(CullMode::Back);

	for(unsigned int m = 0; m < pModel->AiScene->mNumMeshes; m++){

		if(pModel->SetTexture){
			aiMaterial* material = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
			if(!pTexture || !pTexture->m_TextureData){
				aiString texName;
				if(material->GetTexture(aiTextureType_DIFFUSE, 0, &texName) == AI_SUCCESS && texName.length > 0){
					auto it = pModel->m_Texture.find(texName.C_Str());
					if(it != pModel->m_Texture.end()){
						deviceContext->PSSetShaderResources(0, 1, &it->second);
					}
				}
				if(material->GetTexture(aiTextureType_NORMALS, 0, &texName) == AI_SUCCESS && texName.length > 0){
					auto it = pModel->m_Texture.find(texName.C_Str());
					if(it != pModel->m_Texture.end()){
						deviceContext->PSSetShaderResources(1, 1, &it->second);
					}
				}
			}
		}
		if(!pTexture){
			MATERIAL materialData;
			aiMaterial* aiMat = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
			aiColor4D color;
			if(aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
				materialData.Diffuse = {color.r, color.g, color.b, color.a};
			if(aiMat->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
				materialData.Ambient = {color.r, color.g, color.b, color.a};
			if(aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
				materialData.Emission = {color.r, color.g, color.b, color.a};
			if(aiMat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
				materialData.Specular = {color.r, color.g, color.b, color.a};

			float shininess = 0.0f;
			if(aiMat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
				materialData.Shininess = std::clamp(shininess, 1.0f, 128.0f);

			materialData.DiffuseTextureEnable = pModel->SetTexture;
			graphicsContext->SetMaterial(materialData);
		} else{

			MATERIAL material = pTexture->Material;
			material.DiffuseTextureEnable = true;
			graphicsContext->SetMaterial(material);

			if(pTexture->m_TextureData){
				deviceContext->PSSetShaderResources(0, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
			}
		}
		graphicsContext->SetWorldMatrix(World);

		// 頂点バッファ設定
		UINT stride = sizeof(VERTEX_3D);
		UINT offset = 0;
		graphicsContext->GetDeviceContext()->IASetVertexBuffers(0, 1, &pModel->VertexBuffer[m], &stride, &offset);

		// インデックスバッファ設定
		graphicsContext->GetDeviceContext()->IASetIndexBuffer(pModel->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		deviceContext->DrawIndexed(pModel->AiScene->mMeshes[m]->mNumFaces * 3, 0, 0);
	}
}
