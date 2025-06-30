// Engine/Scene/System/renderSystem.cpp
#include "renderSystem.h"
#include <DirectXMath.h>
#include "Backends/DirectX11/DirectXTex.h"
#include "Backends/ImGui/ImGui.h"
#include "Backends/ImGui/ImGuizmo.h"
#include "Backends/myVector3.h"

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
#include <Component/bumpMapComponent.h>

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

	device->CreateTexture2D(&td, nullptr, &tex_editor);
	device->CreateRenderTargetView(tex_editor, nullptr, &rtv_editor);
	device->CreateShaderResourceView(tex_editor, nullptr, &srv_editor);

	device->CreateTexture2D(&td, nullptr, &tex_player);
	device->CreateRenderTargetView(tex_player, nullptr, &rtv_player);
	device->CreateShaderResourceView(tex_player, nullptr, &srv_player);

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
		hr = device->CreateDepthStencilView(depthStencile, &depthStencilViewDesc, &dsv_editor);
		hr = device->CreateDepthStencilView(depthStencile, &depthStencilViewDesc, &dsv_player);
		depthStencile->Release();

		if(FAILED(hr)){
			return;
		}
	}

	m_billBoardMesh = new MeshRendererComponent;
	if(m_billBoardMesh){

		m_billBoardMesh->mesh.meshCount = 4;
		VERTEX_3D vertex[4]{};

		vertex[0].Position = DirectX::XMFLOAT3(-0.5f, 0.5f, 0.0f);
		vertex[0].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[0].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[0].TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);

		vertex[1].Position = DirectX::XMFLOAT3(0.5f, 0.5f, 0.0f);
		vertex[1].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[1].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[1].TexCoord = DirectX::XMFLOAT2(1.0f, 0.0f);

		vertex[2].Position = DirectX::XMFLOAT3(-0.5f, -0.5f, 0.0f);
		vertex[2].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[2].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[2].TexCoord = DirectX::XMFLOAT2(0.0f, 1.0f);

		vertex[3].Position = DirectX::XMFLOAT3(0.5f, -0.5f, 0.0f);
		vertex[3].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[3].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[3].TexCoord = DirectX::XMFLOAT2(1.0f, 1.0f);

		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(VERTEX_3D) * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = vertex;

		m_context->manager->renderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, m_billBoardMesh->mesh.m_VertexBuffer.GetAddressOf());
		m_context->manager->renderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\commonVS.cso", m_billBoardMesh->mesh.m_VertexShader.GetAddressOf(), m_billBoardMesh->mesh.m_VertexLayout.GetAddressOf());
		m_context->manager->renderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\unlitUVTexturePS.cso", m_billBoardMesh->mesh.m_PixelShader.GetAddressOf());
	}

	m_SpriteMesh = new MeshRendererComponent;
	if (m_SpriteMesh) {

		m_SpriteMesh->mesh.meshCount = 4;
		VERTEX_3D vertex[4]{};

		vertex[0].Position = DirectX::XMFLOAT3(0.5f, 0.5f, 0.0f);
		vertex[0].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[0].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[0].TexCoord = DirectX::XMFLOAT2(1.0f, 1.0f);

		vertex[1].Position = DirectX::XMFLOAT3(-0.5f, 0.5f, 0.0f);
		vertex[1].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[1].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[1].TexCoord = DirectX::XMFLOAT2(0.0f, 1.0f);

		vertex[2].Position = DirectX::XMFLOAT3(0.5f, -0.5f, 0.0f);
		vertex[2].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[2].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[2].TexCoord = DirectX::XMFLOAT2(1.0f, 0.0f);

		vertex[3].Position = DirectX::XMFLOAT3(-0.5f, -0.5f, 0.0f);
		vertex[3].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[3].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[3].TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);

		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(VERTEX_3D) * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = vertex;

		m_context->manager->renderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, m_SpriteMesh->mesh.m_VertexBuffer.GetAddressOf());
		m_context->manager->renderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\commonVS.cso", m_SpriteMesh->mesh.m_VertexShader.GetAddressOf(), m_SpriteMesh->mesh.m_VertexLayout.GetAddressOf());
		m_context->manager->renderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\unlitUVTexturePS.cso", m_SpriteMesh->mesh.m_PixelShader.GetAddressOf());
	}
}

void RenderSystem::Finalize(){
	delete m_billBoardMesh;
	delete m_SpriteMesh;
	tex_editor->Release();
	rtv_editor->Release();
	srv_editor->Release();
	dsv_editor->Release();

	tex_player->Release();
	rtv_player->Release();
	srv_player->Release();
	dsv_player->Release();
}

void RenderSystem::Draw(){




	//DrawEntities();

	PlayerView();

	EditorView();
}

void RenderSystem::EditorUpdate(float deltaTime){




	ImGuiIO& io = ImGui::GetIO();

	// マウス右クリックで操作有効
	static bool isCameraActive = false;
	if(ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		isCameraActive = true;
	else if(!ImGui::IsMouseDown(ImGuiMouseButton_Right))
		isCameraActive = false;

	// 入力で動かすベクトル
	Vector3 velocity = {0,0,0};
	float speed = 20.0f;

	if(isCameraActive){
		// ------ 1. マウス移動で回転 ------
		float mouseSensitivity = 0.005f;
		m_EditorCameraRotation.x += io.MouseDelta.x * mouseSensitivity; // yaw
		m_EditorCameraRotation.y -= io.MouseDelta.y * mouseSensitivity; // pitch

		// ピッチ制限
		const float pitchLimit = DirectX::XM_PIDIV2 - 0.01f;
		if(m_EditorCameraRotation.y > pitchLimit) m_EditorCameraRotation.y = pitchLimit;
		if(m_EditorCameraRotation.y < -pitchLimit) m_EditorCameraRotation.y = -pitchLimit;

		// 回転から方向ベクトル取得
		Vector3 front;
		front.x = cosf(m_EditorCameraRotation.y) * sinf(m_EditorCameraRotation.x);
		front.y = 0.0f;
		front.z = cosf(m_EditorCameraRotation.y) * cosf(m_EditorCameraRotation.x);
		front = front.normalize();
		Vector3 right = (Vec3Cross(front, Vector3(0.0f, 1.0f, 0.0f))).normalize();
		Vector3 up = (Vec3Cross(right, front)).normalize();

		if(ImGui::IsKeyDown(ImGuiKey_W)) velocity += front;
		if(ImGui::IsKeyDown(ImGuiKey_S)) velocity -= front;
		if(ImGui::IsKeyDown(ImGuiKey_A)) velocity += right;
		if(ImGui::IsKeyDown(ImGuiKey_D)) velocity -= right;
		if(ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_LeftShift)) velocity -= up;
		if(ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) velocity += up;
		if(velocity.length() > 0.0f){
			m_EditorCameraPosition += velocity.normalize() * speed * deltaTime;
		}
	} else{
		// 回転から方向ベクトル取得
		Vector3 front;
		front.x = cosf(m_EditorCameraRotation.y) * sinf(m_EditorCameraRotation.x);
		front.y = sinf(m_EditorCameraRotation.y);
		front.z = cosf(m_EditorCameraRotation.y) * cosf(m_EditorCameraRotation.x);
		front = front.normalize();
		Vector3 right = (Vec3Cross(front, Vector3(0.0f, 1.0f, 0.0f))).normalize();
		Vector3 up = (Vec3Cross(right, front)).normalize();

		// ------ 3. 中クリックでパン移動 ------
		if(ImGui::IsMouseDown(ImGuiMouseButton_Middle)){
			float panSensitivity = 0.1f;
			m_EditorCameraPosition += right * io.MouseDelta.x * panSensitivity;
			m_EditorCameraPosition += up * io.MouseDelta.y * panSensitivity;
		}
		// ------ 4. マウスホイールでズーム ------
		if(m_MouseWheel != 0.0f){
			float zoomSensitivity = 5.0f; // ズーム速度
			m_EditorCameraPosition += front * m_MouseWheel * zoomSensitivity;
		}
	}
}

void RenderSystem::DrawMesh(TransformComponent* transform, MeshRendererComponent* meshRenderer, TextureComponent* pTexture){


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

	DirectX::XMMATRIX World = Scale * Rotation * Translation;

	graphicsContext->SetWorldViewProjection2D();
	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	deviceContext->Draw(meshRenderer->mesh.meshCount, 0);

	graphicsContext->SetDepthEnable(true);
}

void RenderSystem::DrawModel(TransformComponent* transform, ModelRendererComponent* modelRenderer, TextureComponent* pTexture){

	ModelData* pModel = modelRenderer->model;
	if(!pModel || !pModel->AiScene){
		m_context->manager->debug->LOG_ERROR("ModelData is null or AiScene is not initialized.");
		return;
	}

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

	DirectX::XMMATRIX InvViewBillBoardMatrix;

	if(billBoard->FreezeXZ){
		// カメラの向きを取得（ビュー行列の逆行列 = ワールド行列）
		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, m_EditorCameraView);

		// Y軸回転だけを取り出す処理
		DirectX::XMFLOAT4X4 invViewFloat4x4;
		DirectX::XMStoreFloat4x4(&invViewFloat4x4, invView);

		// 上3行3列だけ使う（位置成分除く）
		DirectX::XMVECTOR forward = DirectX::XMVectorSet(invViewFloat4x4._31, 0.0f, invViewFloat4x4._33, 0.0f); // Z軸（前）
		forward = DirectX::XMVector3Normalize(forward);

		DirectX::XMVECTOR right = DirectX::XMVector3Cross(DirectX::XMVectorSet(0, 1, 0, 0), forward); // Y軸とZ軸からX軸計算
		right = DirectX::XMVector3Normalize(right);

		DirectX::XMVECTOR up = DirectX::XMVector3Cross(forward, right); // Z軸とX軸からY軸計算（右手系）

		// 回転行列を作る
		InvViewBillBoardMatrix = DirectX::XMMATRIX(
			right,
			up,
			forward,
			DirectX::XMVectorSet(0, 0, 0, 1)
		);
	} else{
		// 通常のビルボード（ビュー行列の回転成分のみ使う）
		DirectX::XMMATRIX InverseViewMatrix = DirectX::XMMatrixTranspose(m_EditorCameraView);
		DirectX::XMFLOAT4X4 Mat;
		DirectX::XMStoreFloat4x4(&Mat, InverseViewMatrix);

		// 位置成分を0に
		Mat._14 = Mat._24 = Mat._34 = 0.0f;
		InvViewBillBoardMatrix = DirectX::XMLoadFloat4x4(&Mat);
	}


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

void RenderSystem::DrawEntities(){
	GraphicsContext* graphicsContext = m_context->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	// コンポーネントを持つエンティティの検索
	const auto& entities = m_context->component->FindEntitiesWithComponent<TransformComponent>();
	if(entities.empty()){
		return;
	} else{
		for(Entity entity : entities){

			TransformComponent* transform = m_context->component->GetComponent<TransformComponent>(entity);
			if(transform){
				BumpMapComponent* bumpMap = m_context->component->GetComponent<BumpMapComponent>(entity);
				if(bumpMap){
					// バンプマップの設定
					if(bumpMap->m_TextureData){
						deviceContext->PSSetShaderResources(1, 1, bumpMap->m_TextureData->pTexture.GetAddressOf());
					} else{
						ID3D11ShaderResourceView* nullSRV = nullptr;
						deviceContext->PSSetShaderResources(1, 1, &nullSRV);
					}
				} else{
					ID3D11ShaderResourceView* nullSRV = nullptr;
					deviceContext->PSSetShaderResources(1, 1, &nullSRV);
				}

				TextureComponent* texture = m_context->component->GetComponent<TextureComponent>(entity);
				if(texture){
					if(texture->m_TextureData){
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

				} else{
					// マテリアル設定
					MATERIAL material{};
					material.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
					graphicsContext->SetMaterial(material);

					UVMatrix uv;
					graphicsContext->SetUVMatrix(uv);
				}

				BillBoardRendererComponent* billBoardRenderer = m_context->component->GetComponent<BillBoardRendererComponent>(entity);
				MeshRendererComponent* meshRenderer = m_context->component->GetComponent<MeshRendererComponent>(entity);
				if(billBoardRenderer){
					DrawBillBoard(transform, m_billBoardMesh, billBoardRenderer, texture);
				} else if(meshRenderer){
					DrawMesh(transform, m_SpriteMesh, texture);
				}

				ModelRendererComponent* modelRenderer = m_context->component->GetComponent<ModelRendererComponent>(entity);
				if(modelRenderer){
					DrawModel(transform, modelRenderer, texture);
				}
			}
		}
	}
}

void RenderSystem::SetCameraView(){

	// コンテキストの取得
	GraphicsContext* graphicsContext = m_context->manager->renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	// カメラコンポーネントを持つエンティティ取得
	auto entities = m_context->component->FindEntitiesWithComponent<CameraComponent>();
	if(entities.empty()){
		//取得に失敗
		return;
	}

	// カメラコンポーネントの取得
	auto cameraComponent = m_context->component->GetComponent<CameraComponent>(entities[0]);
	// トランスフォームの取得
	auto transformComponent = m_context->component->GetComponent<TransformComponent>(entities[0]);

	// プロジェクションマトリクス設定
	DirectX::XMMATRIX projection;
	projection = DirectX::XMMatrixPerspectiveFovLH(cameraComponent->FOV, (float)graphicsContext->m_width / graphicsContext->m_height, cameraComponent->NearClip, cameraComponent->FarClip);
	graphicsContext->SetProjectionMatrix(projection);

	// コンスタントバッファ設定
	Vector3 position = transformComponent->position;
	CAMERA camera{};
	camera.CameraPosition = {position.x,position.y,position.z,0.0f};
	graphicsContext->SetCamera(camera);


	// ビューマトリクス設定
	if(cameraComponent->isLock){

		Vector3 target = cameraComponent->Target;
		cameraComponent->viewMatrix = DirectX::XMMatrixLookAtLH({position.x,position.y,position.z}, {target.x,target.y,target.z}, {0.0f,1.0f,0.0f});


	} else{

		Vector3 front = transformComponent->position + transformComponent->front().normalize();
		Vector3 up = transformComponent->position + transformComponent->up().normalize();

		cameraComponent->viewMatrix = DirectX::XMMatrixLookAtLH({position.x,position.y,position.z}, {front.x,front.y,front.z}, {0.0f,1.0f,0.0f});
	}
	graphicsContext->SetViewMatrix(cameraComponent->viewMatrix);
	m_EditorCameraView = cameraComponent->viewMatrix;
	m_context->manager->imgui->SetViewProjectionMatrix(cameraComponent->viewMatrix, projection);
}

void RenderSystem::SetEditorCameraView(){
	// コンテキストの取得
	GraphicsContext* graphicsContext = m_context->manager->renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	// プロジェクションマトリクス設定
	DirectX::XMMATRIX projection;
	projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, (float)graphicsContext->m_width / graphicsContext->m_height, 0.01f, 1000.0f);
	graphicsContext->SetProjectionMatrix(projection);
	// ビューマトリクス設定
	float pitch = m_EditorCameraRotation.y;
	float yaw = m_EditorCameraRotation.x;

	Vector3 front;
	front.x = cosf(pitch) * sinf(yaw);
	front.y = sinf(pitch);
	front.z = cosf(pitch) * cosf(yaw);

	// コンスタントバッファ設定
	Vector3 position = m_EditorCameraPosition;
	CAMERA camera{};
	camera.CameraPosition = {position.x,position.y,position.z,0.0f};
	graphicsContext->SetCamera(camera);


	DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(m_EditorCameraPosition.ToXMVECTOR(), (m_EditorCameraPosition + front).ToXMVECTOR(), {0.0f, 1.0f, 0.0f});
	graphicsContext->SetViewMatrix(view);
	m_context->manager->imgui->SetViewProjectionMatrix(view, projection);
	m_EditorCameraView = view;
}

void RenderSystem::EditorView(){
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

	deviceContext->ClearRenderTargetView(rtv_editor, clearCol);

	deviceContext->ClearDepthStencilView(dsv_editor, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	graphicsContext->GetDeviceContext()->OMSetRenderTargets(1, &rtv_editor, dsv_editor);
	SetEditorCameraView();

	DrawEntities();

	ImGui::Begin("Editor View");



	// ツールバー内容
	if(ImGui::Button("Play")){
		m_context->state = SceneState::Playing; // シーンの状態を再生中に変更
	}
	ImGui::SameLine();
	if(ImGui::Button("Stop")){
		m_context->state = SceneState::Stopped; // シーンの状態をエディタに戻す
	}
	ImGui::Separator();

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
	ImGui::Image((ImTextureID)srv_editor, dst, ImVec2(0, 0), ImVec2(1, 1));

	ImGuizmo::SetRect(
		ImGui::GetWindowPos().x + cursor.x,
		ImGui::GetWindowPos().y + cursor.y,
		dst.x,
		dst.y
	);
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

	// ImGuiIO を取得
	ImGuiIO& io = ImGui::GetIO();

	// マウスホイールの値をリセット
	m_MouseWheel = 0.0f;

	// マウスカーソルの位置を取得
	POINT cursorPos;
	if(GetCursorPos(&cursorPos)){
		// 現在のウィンドウのハンドルを取得
		HWND hwnd = GetActiveWindow();
		if(hwnd){
			// スクリーン座標をウィンドウのクライアント座標に変換
			ScreenToClient(hwnd, &cursorPos);

			// 現在のウィンドウの描画リストを取得
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 mousePos = io.MousePos;

			// マウスカーソルがウィンドウ内にあるかを判定
			if(drawList->GetClipRectMin().x <= mousePos.x && mousePos.x <= drawList->GetClipRectMax().x &&
			   drawList->GetClipRectMin().y <= mousePos.y && mousePos.y <= drawList->GetClipRectMax().y){
				// マウスホイールの入力を取得
				m_MouseWheel = io.MouseWheel;
			}
		}
	}

	ImGui::End();

	graphicsContext->GetDeviceContext()->OMSetRenderTargets(1, graphicsContext->GetpRenderTargetView(), graphicsContext->GetDepthStencilView());
}

void RenderSystem::PlayerView(){

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

	deviceContext->ClearRenderTargetView(rtv_player, clearCol);

	deviceContext->ClearDepthStencilView(dsv_player, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	graphicsContext->GetDeviceContext()->OMSetRenderTargets(1, &rtv_player, dsv_player);

	// カメラビューの設定
	SetCameraView();

	// エンティティの描画
	DrawEntities();

	ImGui::Begin("Play View");

	// ツールバー内容
	if(ImGui::Button("Play")){
		m_context->state = SceneState::Playing; // シーンの状態を再生中に変更
	}
	ImGui::SameLine();
	if(ImGui::Button("Stop")){
		m_context->state = SceneState::Stopped; // シーンの状態をエディタに戻す
	}
	ImGui::Separator();

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
	ImGui::Image((ImTextureID)srv_player, dst, ImVec2(0, 0), ImVec2(1, 1));

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
