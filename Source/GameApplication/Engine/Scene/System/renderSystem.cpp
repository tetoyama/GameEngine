// Engine/Scene/System/renderSystem.cpp
#include "renderSystem.h"
#include <DirectXMath.h>
#include "Backends/DirectX11/DirectXTex.h"
#include "Backends/ImGui/ImGui.h"
#include "Backends/ImGui/ImGuizmo.h"

#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"
#include "Backends/Assimp/matrix4x4.h"
#include "Engine/DebugTools/debugSystem.h"

#include "Engine/Resources/Data/modelData.h"

#include "Registry/entityRegistry.h"
#include "Registry/componentRegistry.h"

#include "Component/transformComponent.h"

#include "Component/textureComponent.h"
#include "Component/meshRendererComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/BillBoardRendererComponent.h"
#include "Component/cameraComponent.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Resources/resourceSystem.h"
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"

#include "Scene.h"
#include "SceneManager.h"

void RenderSystem::Initialize(){
	m_context->manager->debug->LOG_DEBUG("RenderSystemを初期化中...");

	D3D11_TEXTURE2D_DESC td = {};
	td.Width = 1280; td.Height = 720;
	td.MipLevels = 1; td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	GraphicsContext* graphicsContext = m_context->manager->graphics;
	ID3D11Device* device = graphicsContext->GetDevice();

	device->CreateTexture2D(&td, nullptr, &tex);
	device->CreateRenderTargetView(tex, nullptr, &rtv);
	device->CreateShaderResourceView(tex, nullptr, &srv);

	// デプスステンシルバッファ作成
	ID3D11Texture2D* depthStencile{};
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width =1280;
	textureDesc.Height = 720;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;
	HRESULT hr = device->CreateTexture2D(&textureDesc, NULL, &depthStencile);
	if(FAILED(hr)){
		return;
	}

	// デプスステンシルビュー作成
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
	depthStencilViewDesc.Format = textureDesc.Format;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Flags = 0;
	if(depthStencile){
		hr = device->CreateDepthStencilView(depthStencile, &depthStencilViewDesc, &dsv);
		depthStencile->Release();

		if(FAILED(hr)){
			return;
		}
	}
}

void RenderSystem::Finalize(){
	tex->Release();
	rtv->Release();
	srv->Release();
	dsv->Release();
}

void RenderSystem::Draw(){




	GraphicsContext* graphicsContext = m_context->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	float clearCol[4] = {0.1f, 0.1f, 0.1f, 1.0f};

	D3D11_VIEWPORT vp = {};
	vp.Width = 1280.0f;
	vp.Height = 720.0f;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	deviceContext->RSSetViewports(1, &vp);

	deviceContext->ClearRenderTargetView(rtv, clearCol);

	deviceContext->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	graphicsContext->GetDeviceContext()->OMSetRenderTargets(1, &rtv, dsv);

	// コンポーネントを持つエンティティの検索
	const auto& entities = m_context->component->FindEntitiesWithComponent<TransformComponent>();
	if (entities.empty()) {
		return;
	} else {
		for (Entity entity : entities) {

			TransformComponent* transform = m_context->component->GetComponent<TransformComponent>(entity);
			if (transform) {

				TextureComponent* texture = m_context->component->GetComponent<TextureComponent>(entity);
				if (texture) {
					if (texture->m_TextureData) {
						deviceContext->PSSetShaderResources(0, 1, texture->m_TextureData->pTexture.GetAddressOf());
					}
					// マテリアル設定
					MATERIAL material{};
					material.Diffuse = texture->Material.Diffuse;
					graphicsContext->SetMaterial(material);

					UVMatrix uv;
					uv.Start.x = (float)(texture->AnimationNum % texture->UV_Slice_Y) * 1.0f / (float)texture->UV_Slice_X;
					uv.Start.y = (float)(texture->AnimationNum / texture->UV_Slice_X) * 1.0f / (float)texture->UV_Slice_Y;

					uv.End.x = (float)uv.Start.x + 1.0f / (float)texture->UV_Slice_X;
					uv.End.y = (float)uv.Start.y + 1.0f / (float)texture->UV_Slice_Y;

					graphicsContext->SetUVMatrix(uv);

				} else {
										// マテリアル設定
					MATERIAL material{};
					material.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
					graphicsContext->SetMaterial(material);

					UVMatrix uv;
					graphicsContext->SetUVMatrix(uv);
				}

				BillBoardRendererComponent* billBoardRenderer = m_context->component->GetComponent<BillBoardRendererComponent>(entity);
				MeshRendererComponent* meshRenderer = m_context->component->GetComponent<MeshRendererComponent>(entity);
				if (meshRenderer && billBoardRenderer) {
					DrawBillBoard(transform, meshRenderer,billBoardRenderer, texture);
				} else if (meshRenderer) {
					DrawMesh(transform, meshRenderer, texture);

				}

				ModelRendererComponent* modelRenderer = m_context->component->GetComponent<ModelRendererComponent>(entity);
				if (modelRenderer) {
					DrawModel(transform, modelRenderer,texture);
				}
			}
		}
	}





	ImGui::Begin("Editor View");

	// ツールバー内容
	if(ImGui::Button("Play")){
		// 再生開始処理
	}
	ImGui::SameLine();
	if(ImGui::Button("Stop")){
		// 停止処理
	}

	ImVec2 avail = ImGui::GetContentRegionAvail(); // ウィンドウ内の利用可能サイズ

	// テクスチャの元サイズ（例）
	float imgW = 1280.0f, imgH = 720.0f;
	float imgAspect = imgW / imgH;
	float availAspect = avail.x / avail.y;

	ImVec2 dst;
	if(availAspect > imgAspect){
		// 領域が横長 → 高さに合わせ、幅を計算
		dst.y = avail.y;
		dst.x = avail.y * imgAspect;
	} else{
		// 領域が縦長または正方形 → 幅に合わせ、高さを計算
		dst.x = avail.x;
		dst.y = avail.x / imgAspect;
	}

	// 中央寄せ
	ImVec2 cursor = ImGui::GetCursorPos();
	ImGui::SetCursorPosX(cursor.x + (avail.x - dst.x) * 0.5f);
	ImGui::SetCursorPosY(cursor.y + (avail.y - dst.y) * 0.5f);
	cursor = ImGui::GetCursorPos();
	// イメージ表示（UV反転も場合に応じて調整）
	ImGui::Image((ImTextureID)srv, dst, ImVec2(0, 0), ImVec2(1, 1));

	ImGuizmo::SetRect(
		ImGui::GetWindowPos().x + cursor.x,
		ImGui::GetWindowPos().y + cursor.y,
		dst.x,
		dst.y
	);
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

	ImGui::End();


	graphicsContext->GetDeviceContext()->OMSetRenderTargets(1, graphicsContext->GetpRenderTargetView(), graphicsContext->GetDepthStencilView());
}

void RenderSystem::DrawMesh(TransformComponent* transform, MeshRendererComponent* meshRenderer, TextureComponent* pTexture){

	GraphicsContext* graphicsContext = m_context->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	graphicsContext->SetWorldViewProjection2D();

	if (!pTexture || !pTexture->m_TextureData) {
		deviceContext->PSSetShaderResources(0, 1, meshRenderer->mesh.m_TextureData->pTexture.GetAddressOf());

		MATERIAL material{};
		material.Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
			graphicsContext->SetMaterial(material);

	}

	deviceContext->IASetInputLayout(meshRenderer->mesh.m_VertexLayout.Get());
	deviceContext->VSSetShader(meshRenderer->mesh.m_VertexShader.Get(), NULL, 0);
	deviceContext->PSSetShader(meshRenderer->mesh.m_PixelShader.Get(), NULL, 0);

	DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationRollPitchYaw(transform->rotation.x, transform->rotation.y, transform->rotation.z);
	DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(transform->scale.x, transform->scale.y, transform->scale.z);
	DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(transform->position.x, transform->position.y, transform->position.z);
	DirectX::XMMATRIX World = Scale * Rotation * Translation;

	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);




	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	deviceContext->Draw(meshRenderer->mesh.meshCount, 0);

}

void RenderSystem::DrawModel(TransformComponent* transform, ModelRendererComponent* modelRenderer, TextureComponent* pTexture){

	ModelData* pModel = modelRenderer->model;

	GraphicsContext* graphicsContext = m_context->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	if(modelRenderer->pixelShader){
		deviceContext->PSSetShader(modelRenderer->pixelShader->m_PixelShader.Get(), NULL, 0);
	}

	if(modelRenderer->vertexShader){
		deviceContext->IASetInputLayout(modelRenderer->vertexShader->m_VertexLayout.Get());
		deviceContext->VSSetShader(modelRenderer->vertexShader->m_VertexShader.Get(), NULL, 0);
	}


	DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationRollPitchYaw(transform->rotation.x, transform->rotation.y, transform->rotation.z);
	DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(transform->scale.x, transform->scale.y, transform->scale.z);
	DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(transform->position.x, transform->position.y, transform->position.z);

	DirectX::XMMATRIX World = Scale * Rotation * Translation;

	for(unsigned int m = 0; m < pModel->AiScene->mNumMeshes; m++){
		if(pModel->SetTexture){

			//テクスチャ設定
			if (!pTexture || !pTexture->m_TextureData) {
				aiString Texture;
				aiMaterial* aiMaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
				aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &Texture);

				if (aiString("") != Texture) {
					deviceContext->PSSetShaderResources(0, 1, &pModel->Texture[Texture.data]);
				}
			}
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

		if (!pTexture) {
		// マテリアル設定
			MATERIAL material;
			ZeroMemory(&material, sizeof(material));
			aiMaterial* pAiMaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
			aiColor4D diffuse;
			pAiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
			material.Diffuse = DirectX::XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, diffuse.a);
			graphicsContext->SetMaterial(material);
		}
		graphicsContext->SetWorldMatrix(World);

		// ポリゴン描画
		deviceContext->DrawIndexed(pModel->AiScene->mMeshes[m]->mNumFaces * 3, 0, 0);
	}
}

void RenderSystem::DrawBillBoard(TransformComponent* transform, MeshRendererComponent* meshRenderer, BillBoardRendererComponent* billBoard, TextureComponent* pTexture) {

	CameraComponent* cameraComponent = nullptr;
	// カメラの取得
	const auto& cameraEntity = m_context->component->FindEntitiesWithComponent<CameraComponent>();
	if (!cameraEntity.empty()) {
		cameraComponent = m_context->component->GetComponent<CameraComponent>(cameraEntity[0]);
	} else {
		return;
	}


	DirectX::XMMATRIX InverseViewMatrix = DirectX::XMMatrixTranspose(cameraComponent->viewMatrix);
	DirectX::XMFLOAT4X4 Mat;
	DirectX::XMStoreFloat4x4(&Mat, InverseViewMatrix);
	Mat._14 = Mat._24 = Mat._34 = 0.0f;
	DirectX::XMMATRIX InvViewBillBoardMatrix;
	InvViewBillBoardMatrix = DirectX::XMLoadFloat4x4(&Mat);



	GraphicsContext* graphicsContext = m_context->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();


	if (!pTexture) {
		if (meshRenderer->mesh.m_TextureData) {
			deviceContext->PSSetShaderResources(0, 1, meshRenderer->mesh.m_TextureData->pTexture.GetAddressOf());

			MATERIAL material{};
			material.Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
			graphicsContext->SetMaterial(material);
		}
	}

	deviceContext->IASetInputLayout(meshRenderer->mesh.m_VertexLayout.Get());

	deviceContext->VSSetShader(meshRenderer->mesh.m_VertexShader.Get(), NULL, 0);
	deviceContext->PSSetShader(meshRenderer->mesh.m_PixelShader.Get(), NULL, 0);

	DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationRollPitchYaw(transform->rotation.x, transform->rotation.y, transform->rotation.z);
	DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(transform->scale.x, transform->scale.y, transform->scale.z);
	DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(transform->position.x, transform->position.y, transform->position.z);

	DirectX::XMMATRIX World =  Scale * InvViewBillBoardMatrix * Translation;


	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	deviceContext->Draw(meshRenderer->mesh.meshCount, 0);

}
