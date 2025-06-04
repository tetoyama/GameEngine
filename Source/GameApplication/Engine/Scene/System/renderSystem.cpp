// Engine/Scene/System/renderSystem.cpp
#include "renderSystem.h"
#include <DirectXMath.h>
#include "Backends/DirectX11/DirectXTex.h"

#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"
#include "Backends/Assimp/matrix4x4.h"

#include "Engine/Resources/Data/modelData.h"

#include "Entity/entityRegistry.h"

#include "Component/transformComponent.h"

#include "Component/meshRendererComponent.h"
#include "Component/modelRendererComponent.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Resources/resourceSystem.h"
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"
void RenderSystem::Draw(){

	GraphicsContext* graphicsContext = m_renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	const auto& searchComponents = m_registry->GetComponents();

	for(const auto& typePair : searchComponents){

		if(typePair.first == typeid(MeshRendererComponent)){
			for(const auto& entityPair : typePair.second){

				Entity entity = entityPair.first;
				auto* meshRenderer = static_cast<MeshRendererComponent*>(entityPair.second.get());

				if(!meshRenderer || !meshRenderer->mesh){
					continue;
				}
				auto* transform = m_registry->GetComponent<TransformComponent>(entity);
				if(!transform){
					continue;
				}

				DrawMesh(transform, meshRenderer);
			}
		} else if(typePair.first == typeid(ModelRendererComponent)){

			for(const auto& entityPair : typePair.second){

				Entity entity = entityPair.first;
				auto* modelRenderer = static_cast<ModelRendererComponent*>(entityPair.second.get());

				if(!modelRenderer || !modelRenderer->model){
					continue;
				}
				auto* transform = m_registry->GetComponent<TransformComponent>(entity);
				if(!transform){
					continue;
				}

				DrawModel(transform, modelRenderer);
			}
		}
	}
}

void RenderSystem::DrawMesh(TransformComponent* transform, MeshRendererComponent* meshRenderer){

	GraphicsContext* graphicsContext = m_renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	deviceContext->IASetInputLayout(meshRenderer->mesh->m_VertexLayout.Get());

	deviceContext->VSSetShader(meshRenderer->mesh->m_VertexShader.Get(), NULL, 0);
	deviceContext->PSSetShader(meshRenderer->mesh->m_PixelShader.Get(), NULL, 0);

	DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationRollPitchYaw(transform->rotation.x, transform->rotation.y, transform->rotation.z);
	DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(transform->scale.x, transform->scale.y, transform->scale.z);
	DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(transform->position.x, transform->position.y, transform->position.z);

	graphicsContext->SetWorldMatrix(Scale * Rotation * Translation);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;

	deviceContext->IASetVertexBuffers(0, 1, &meshRenderer->mesh->m_VertexBuffer, &stride, &offset);

	deviceContext->PSSetShaderResources(0, 1, &meshRenderer->mesh->m_TextureData->pTexture);

	MATERIAL material{};
	material.Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);

	graphicsContext->SetMaterial(material);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	if(meshRenderer->mesh->m_IndexBuffer){

		deviceContext->IASetIndexBuffer(
			meshRenderer->mesh->m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		deviceContext->DrawIndexed(meshRenderer->mesh->indexCount, 0, 0);

	} else{
		deviceContext->Draw(meshRenderer->mesh->meshCount, 0);
	}
}

void RenderSystem::DrawModel(TransformComponent* transform, ModelRendererComponent* modelRenderer){

	ModelData* pModel = modelRenderer->model.get();

	GraphicsContext* graphicsContext = m_renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	if(modelRenderer->pixelShader){
		deviceContext->PSSetShader(modelRenderer->pixelShader->m_PixelShader.Get(), NULL, 0);
	}

	if(modelRenderer->vertexShader){
		deviceContext->IASetInputLayout(modelRenderer->vertexShader->m_VertexLayout.Get());
		deviceContext->VSSetShader(modelRenderer->vertexShader->m_VertexShader.Get(), NULL, 0);
	}

	for(unsigned int m = 0; m < pModel->AiScene->mNumMeshes; m++){
		if(pModel->SetTexture){

			//テクスチャ設定
			aiString Texture;
			aiMaterial* aiMaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
			aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &Texture);

			if(aiString("") != Texture){
				deviceContext->PSSetShaderResources(0, 1, &pModel->Texture[Texture.data]);
			}
		} else{
			//SetTexture(0);
		}
		// 頂点バッファ設定
		UINT stride = sizeof(VERTEX_3D);
		UINT offset = 0;
		deviceContext->IASetVertexBuffers(0, 1, &pModel->VertexBuffer[m], &stride, &offset);

		//インデックスバッファ生成
		deviceContext->IASetIndexBuffer(pModel->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		// プリミティブトポロジ設定
		deviceContext->IASetPrimitiveTopology(
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST		//渡された頂点の配列をどのように解釈するか
		);

		// マテリアル設定
		MATERIAL material;
		ZeroMemory(&material, sizeof(material));
		aiMaterial* pAiMaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
		aiColor3D diffuse;
		pAiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
		material.Diffuse = DirectX::XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, 1);
		graphicsContext->SetMaterial(material);

		//g_GraphicContext->SetLight(World);

		DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationRollPitchYaw(transform->rotation.x, transform->rotation.y, transform->rotation.z);
		DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(transform->scale.x, transform->scale.y, transform->scale.z);
		DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(transform->position.x, transform->position.y, transform->position.z);

		graphicsContext->SetWorldMatrix(Scale * Rotation * Translation);

		// ポリゴン描画
		deviceContext->DrawIndexed(pModel->AiScene->mMeshes[m]->mNumFaces * 3, 0, 0);
	}
}
