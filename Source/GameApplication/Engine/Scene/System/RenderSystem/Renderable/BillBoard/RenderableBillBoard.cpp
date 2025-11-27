#include "RenderableBillBoard.h"

#include <d3d11.h>
#include "../RenderableContext.h"

#include "GameApplication/Engine/DebugTools/DebugSystem.h"
#include "GameApplication/Engine/Graphics/mainRenderer.h"

#include "GameApplication/Engine/Scene/scene.h"
#include "GameApplication/Engine/Scene/sceneManager.h"
#include "GameApplication/Engine/Scene/Registry/componentRegistry.h"

#include "GameApplication/Engine/Scene/Component/BillBoardRendererComponent.h"
#include "GameApplication/Engine/Scene/Component/meshRendererComponent.h"
#include "GameApplication/Engine/Scene/Component/transformComponent.h"
#include "GameApplication/Engine/Scene/Component/textureComponent.h"

void RenderableBillBoard::Initialize(SceneManagerContext* context){
	m_billBoardMesh = new MeshRendererComponent;
	if(m_billBoardMesh){

		m_billBoardMesh->mesh.meshCount = 4;
		VERTEX_3D vertex[4]{};

		vertex[0].Position = DirectX::XMFLOAT3(-0.5f, 0.5f, 0.0f);
		vertex[0].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[0].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[0].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[0].TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);

		vertex[1].Position = DirectX::XMFLOAT3(0.5f, 0.5f, 0.0f);
		vertex[1].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[1].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[1].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[1].TexCoord = DirectX::XMFLOAT2(1.0f, 0.0f);

		vertex[2].Position = DirectX::XMFLOAT3(-0.5f, -0.5f, 0.0f);
		vertex[2].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[2].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[2].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[2].TexCoord = DirectX::XMFLOAT2(0.0f, 1.0f);

		vertex[3].Position = DirectX::XMFLOAT3(0.5f, -0.5f, 0.0f);
		vertex[3].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[3].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[3].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[3].TexCoord = DirectX::XMFLOAT2(1.0f, 1.0f);

		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(VERTEX_3D) * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = vertex;

		context->renderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, m_billBoardMesh->mesh.m_VertexBuffer.GetAddressOf());
		context->renderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\commonVS.cso", m_billBoardMesh->mesh.m_VertexShader.GetAddressOf(), m_billBoardMesh->mesh.m_VertexLayout.GetAddressOf());
		context->renderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\unlitUVTexturePS.cso", m_billBoardMesh->mesh.m_PixelShader.GetAddressOf());
	}
}

void RenderableBillBoard::Finalize(){
	delete m_billBoardMesh;
}

void RenderableBillBoard::Execute(const RenderableContext& ctx, SceneContext* sceneContext, const Entity& entity){

	BillBoardRendererComponent* billBoard = sceneContext->component->GetComponent<BillBoardRendererComponent>(entity);
	TransformComponent* transform = sceneContext->component->GetComponent<TransformComponent>(entity);
	if(!billBoard || !transform){
		return;
	}
	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11Device* device = graphicsContext->GetDevice();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	ComponentRegistry* componentRegistry = sceneContext->component;

	TextureComponent* pTexture = sceneContext->component->GetComponent<TextureComponent>(entity);
	if(pTexture){
		MATERIAL material = pTexture->Material;
		material.DiffuseTextureEnable = true;
		graphicsContext->SetMaterial(material);

		if(pTexture->m_TextureData){
			deviceContext->PSSetShaderResources(0, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
		}
	}
	DirectX::XMMATRIX InvViewBillBoardMatrix = DirectX::XMMatrixRotationQuaternion(transform->rotationVector());
	DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, ctx.viewMatrix);
	DirectX::XMFLOAT4X4 invViewFloat4x4;
	DirectX::XMStoreFloat4x4(&invViewFloat4x4, invView);

	if(!billBoard->RotateXYZ.x && !billBoard->RotateXYZ.y && !billBoard->RotateXYZ.z){
		// 全軸false：回転しない → 単位行列（通常メッシュと同じ）

	} else if(billBoard->RotateXYZ.y && !billBoard->RotateXYZ.x && !billBoard->RotateXYZ.z){

		// Y軸だけ回転（XZビルボード） → UIパネルなどで多用される
		// カメラの前方向ベクトル（Z軸）
		DirectX::XMVECTOR forward = DirectX::XMVectorSet(invViewFloat4x4._31, 0.0f, invViewFloat4x4._33, 0.0f);
		forward = DirectX::XMVector3Normalize(forward);

		// up = Y軸固定
		DirectX::XMVECTOR up = DirectX::XMVectorSet(0, 1, 0, 0);
		DirectX::XMVECTOR right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, forward));
		up = DirectX::XMVector3Cross(forward, right);

		InvViewBillBoardMatrix = DirectX::XMMATRIX(
			right,
			up,
			forward,
			DirectX::XMVectorSet(0, 0, 0, 1)
		);
	} else{
		// 任意の軸制御（全軸、Y+Z など）
		DirectX::XMVECTOR forward = DirectX::XMVectorSet(invViewFloat4x4._31, invViewFloat4x4._32, invViewFloat4x4._33, 0.0f);
		DirectX::XMVECTOR right = DirectX::XMVectorSet(invViewFloat4x4._11, invViewFloat4x4._12, invViewFloat4x4._13, 0.0f);
		DirectX::XMVECTOR up = DirectX::XMVectorSet(invViewFloat4x4._21, invViewFloat4x4._22, invViewFloat4x4._23, 0.0f);

		const DirectX::XMVECTOR worldRight = DirectX::XMVectorSet(1, 0, 0, 0);
		const DirectX::XMVECTOR worldUp = DirectX::XMVectorSet(0, 1, 0, 0);
		const DirectX::XMVECTOR worldForward = DirectX::XMVectorSet(0, 0, 1, 0);

		if(!billBoard->RotateXYZ.x) right = worldRight;
		if(!billBoard->RotateXYZ.y) up = worldUp;
		if(!billBoard->RotateXYZ.z) forward = worldForward;

		// 再直交化（forward優先）
		forward = DirectX::XMVector3Normalize(forward);
		right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, forward));
		up = DirectX::XMVector3Cross(forward, right);

		InvViewBillBoardMatrix = DirectX::XMMATRIX(
			right,
			up,
			forward,
			DirectX::XMVectorSet(0, 0, 0, 1)
		);
	}

	if(!pTexture){
		if(m_billBoardMesh->mesh.m_TextureData){
			deviceContext->PSSetShaderResources(0, 1, m_billBoardMesh->mesh.m_TextureData->pTexture.GetAddressOf());

			MATERIAL material{};
			material.Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
			graphicsContext->SetMaterial(material);
		}
	}

	deviceContext->IASetInputLayout(m_billBoardMesh->mesh.m_VertexLayout.Get());

	deviceContext->VSSetShader(m_billBoardMesh->mesh.m_VertexShader.Get(), NULL, 0);
	deviceContext->PSSetShader(m_billBoardMesh->mesh.m_PixelShader.Get(), NULL, 0);

	// ローカル変換行列（スケール・ビルボード回転・位置）
	DirectX::XMMATRIX LocalMatrix =
		DirectX::XMMatrixScaling(transform->scale.x, transform->scale.y, transform->scale.z) *
		InvViewBillBoardMatrix *
		DirectX::XMMatrixTranslation(transform->position.x, transform->position.y, transform->position.z);

	DirectX::XMMATRIX WorldMatrix = LocalMatrix;

	if(transform->parent != 0){
		auto parentTransform = componentRegistry->GetComponent<TransformComponent>(transform->parent);
		if(parentTransform){
			DirectX::XMMATRIX parentWorld = parentTransform->CalculateWorldMatrix(parentTransform, componentRegistry);

			// 親の位置だけを取得（分解）
			DirectX::XMVECTOR parentScale, parentRotation, parentTranslation;
			DirectX::XMMatrixDecompose(&parentScale, &parentRotation, &parentTranslation, parentWorld);

			// 親の位置だけの平行移動行列を作る
			DirectX::XMMATRIX parentTranslationMatrix = DirectX::XMMatrixTranslationFromVector(parentTranslation);

			// 親の回転・スケールは無視し、親位置だけ足す
			WorldMatrix = LocalMatrix * parentTranslationMatrix;
		}
	}

	graphicsContext->SetWorldMatrix(WorldMatrix);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;

	deviceContext->IASetVertexBuffers(0, 1, m_billBoardMesh->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	if (ctx.passPhase == RenderPhase::PHASE_SHADOW) {
		deviceContext->PSSetShader(nullptr, NULL, 0); // ピクセルシェーダー無効化
	}

	deviceContext->Draw(m_billBoardMesh->mesh.meshCount, 0);
}
