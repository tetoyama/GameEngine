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

void RenderableModel::Execute(
	const RenderPassContext& ctx,
	SceneContext* sceneContext,
	const Entity& entity){
	ModelRendererComponent* modelRenderer =
		sceneContext->component->GetComponent<ModelRendererComponent>(entity);

	TransformComponent* transform =
		sceneContext->component->GetComponent<TransformComponent>(entity);

	if(!modelRenderer || !transform){
		return;
	}

	ModelData* model = modelRenderer->model.get();

	if(!model || !model->AiScene){
		return;
	}

	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	TextureComponent* textureComponent =
		sceneContext->component->GetComponent<TextureComponent>(entity);

	MaterialComponent* materialComponent =
		sceneContext->component->GetComponent<MaterialComponent>(entity);


	//----------------------------------------------------------------------
	// Material
	//----------------------------------------------------------------------

	MATERIAL material{};
	material.BaseColor = {1.0f, 1.0f, 1.0f, 1.0f};

	if(materialComponent){
		material = materialComponent->Material;

		// ユーザーが設定したフラグのみ保持する
		// テクスチャ有無によるフラグは後段で自動設定する
		material.MaterialFlags &= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
	}

	//----------------------------------------------------------------------
	// Texture / UV Animation
	//----------------------------------------------------------------------

	UVMatrixBuffer uv{};
	uv.UVStart = float2(0.0f, 0.0f);
	uv.UVEnd = float2(1.0f, 1.0f);

	if(textureComponent){
		if(textureComponent->m_TextureData){
			material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;

			deviceContext->PSSetShaderResources(
				TextureSlot_Albedo,
				1,
				textureComponent->m_TextureData->pTexture.GetAddressOf());
		}

		if(textureComponent->UV_Slice_X > 0.0f && textureComponent->UV_Slice_Y > 0.0f){
			// UV_Slice_X/Y は「1セルのUVサイズ」
			// 例:
			// 0.25f = 4分割
			// 0.125f = 8分割

			int column = (int)(textureComponent->UV_Slice_X);
			if(column <= 0){
				column = 1;
			}

			uv.UVStart.x = (textureComponent->AnimationNum % column) * textureComponent->UV_Slice_X;
			uv.UVStart.y = (textureComponent->AnimationNum / column) * textureComponent->UV_Slice_Y;

			// 1 セルの UV サイズ: 1/スライス数
			uv.UVEnd.x = uv.UVStart.x + 1.0f / textureComponent->UV_Slice_X;  // セルの右端 UV
			uv.UVEnd.y = uv.UVStart.y + 1.0f / textureComponent->UV_Slice_Y;  // セルの下端 UV
		}
	}
	graphicsContext->SetUVMatrixBuffer(uv);
	graphicsContext->SetMaterial(material);

	//----------------------------------------------------------------------
	// Render State
	//----------------------------------------------------------------------

	deviceContext->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	graphicsContext->SetCullMode(CullMode::Back);

	DirectX::XMMATRIX world =
		transform->CalculateWorldMatrix(
			transform,
			sceneContext->component);
	graphicsContext->SetWorldMatrix(world);

	//----------------------------------------------------------------------
	// Draw Mesh
	//----------------------------------------------------------------------

	for(UINT meshIndex = 0;
		meshIndex < model->AiScene->mNumMeshes;
		++meshIndex){

		if(!textureComponent || !textureComponent->m_TextureData){


			MATERIAL materialData = material;

			// ユーザー指定フラグのみ残す
			materialData.MaterialFlags &=
				MATERIAL_FLAG_USE_ENVIRONMENT_MAP;

			aiMesh* mesh =
				model->AiScene->mMeshes[meshIndex];

			aiMaterial* aiMaterial =
				model->AiScene->mMaterials[mesh->mMaterialIndex];

			//------------------------------------------------------------------
			// Diffuse Color
			//------------------------------------------------------------------

			aiColor4D color;

			if(aiMaterial->Get(
				AI_MATKEY_COLOR_DIFFUSE,
				color) == AI_SUCCESS){
				materialData.BaseColor =
				{
					color.r,
					color.g,
					color.b,
					color.a
				};

				if(materialComponent){
					materialData.BaseColor.x *=
						materialComponent->Material.BaseColor.x;

					materialData.BaseColor.y *=
						materialComponent->Material.BaseColor.y;

					materialData.BaseColor.z *=
						materialComponent->Material.BaseColor.z;

					materialData.BaseColor.w *=
						materialComponent->Material.BaseColor.w;
				}
			}

			//------------------------------------------------------------------
			// Diffuse Texture
			//------------------------------------------------------------------

			aiString texName;

			if(aiMaterial->GetTexture(
				aiTextureType_DIFFUSE,
				0,
				&texName) == AI_SUCCESS &&
				texName.length > 0){
				auto it =
					model->m_Texture.find(texName.C_Str());

				if(it != model->m_Texture.end()){
					deviceContext->PSSetShaderResources(
						TextureSlot_Albedo,
						1,
						&it->second);

					materialData.MaterialFlags |=
						MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
				}
			}

			//------------------------------------------------------------------
			// Normal Map
			//------------------------------------------------------------------
			if(ctx.passPhase != RenderPhase::PHASE_SHADOW){

				if(aiMaterial->GetTexture(
					aiTextureType_NORMALS,
					0,
					&texName) == AI_SUCCESS &&
					texName.length > 0){
					auto it =
						model->m_Texture.find(texName.C_Str());

					if(it != model->m_Texture.end()){
						deviceContext->PSSetShaderResources(
							1,
							1,
							&it->second);

						materialData.MaterialFlags |=
							MATERIAL_FLAG_USE_NORMAL_TEXTURE;
					}
				}

				graphicsContext->SetMaterial(materialData);
			}
		} else{
			if(model->SetTexture){
				material.MaterialFlags |=
					MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
			}
			if(ctx.passPhase != RenderPhase::PHASE_SHADOW){
				deviceContext->PSSetShaderResources(
					TextureSlot_Albedo,
					1,
					textureComponent->m_TextureData->pTexture.GetAddressOf());
				graphicsContext->SetMaterial(material);
			}
		}

		//------------------------------------------------------------------
		// Vertex Buffer
		//------------------------------------------------------------------

		UINT stride = sizeof(VERTEX_3D);
		UINT offset = 0;

		if(!modelRenderer->blendedAnimations.empty()){
			deviceContext->IASetVertexBuffers(
				0,
				1,
				&modelRenderer->dynamicVertexBuffers[meshIndex],
				&stride,
				&offset);
		} else{
			deviceContext->IASetVertexBuffers(
				0,
				1,
				&model->VertexBuffer[meshIndex],
				&stride,
				&offset);
		}

		//------------------------------------------------------------------
		// Index Buffer
		//------------------------------------------------------------------

		deviceContext->IASetIndexBuffer(
			model->IndexBuffer[meshIndex],
			DXGI_FORMAT_R32_UINT,
			0);

		//------------------------------------------------------------------
		// Draw
		//------------------------------------------------------------------

		deviceContext->DrawIndexed(
			model->AiScene->mMeshes[meshIndex]->mNumFaces * 3,
			0,
			0);
	}
}