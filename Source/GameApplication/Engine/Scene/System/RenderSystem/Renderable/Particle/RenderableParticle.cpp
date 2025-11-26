#include "RenderableParticle.h"
#include <d3d11.h>
#include "../RenderableContext.h"

#include "GameApplication/Engine/DebugTools/DebugSystem.h"
#include "GameApplication/Engine/Graphics/mainRenderer.h"

#include "GameApplication/Engine/Scene/scene.h"
#include "GameApplication/Engine/Scene/sceneManager.h"
#include "GameApplication/Engine/Scene/Registry/componentRegistry.h"

#include "GameApplication/Engine/Scene/Component/particleComponent.h"
#include "GameApplication/Engine/Scene/Component/BillBoardRendererComponent.h"
#include "GameApplication/Engine/Scene/Component/meshRendererComponent.h"
#include "GameApplication/Engine/Scene/Component/transformComponent.h"
#include "GameApplication/Engine/Scene/Component/textureComponent.h"
void RenderableParticle::Initialize(SceneManagerContext* context){
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

void RenderableParticle::Finalize(){
	delete m_billBoardMesh;
}

void RenderableParticle::Execute(const RenderableContext& ctx, SceneContext* sceneContext, const Entity& entity){
	ParticleComponent* pParticle = sceneContext->component->GetComponent<ParticleComponent>(entity);
	TransformComponent* pTransform = sceneContext->component->GetComponent<TransformComponent>(entity);
	if(!pParticle || !pTransform){
		return;
	}
	TextureComponent* pTexture = sceneContext->component->GetComponent<TextureComponent>(entity);

	GraphicsContext* graphicsContext = sceneContext->manager->renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	graphicsContext->SetBlendMode(BlendMode::Additive);

	for(int i = 0; i < MAXPARTICLE; i++){
		if(pParticle->Particle[i].LifeTime > 0.0f){
			MATERIAL material;

			if(pTexture){
				// マテリアル設定
				material.Diffuse = pTexture->Material.Diffuse;
				material.DiffuseTextureEnable = ((bool)pTexture->m_TextureData);
				if(pTexture->m_TextureData){
					deviceContext->PSSetShaderResources(0, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
				}

				UVMatrix uv;
				if(pTexture->UV_Slice_X != 0 && pTexture->UV_Slice_Y != 0){
					uv.Start.x = (float)(pTexture->AnimationNum % pTexture->UV_Slice_X) * 1.0f / (float)pTexture->UV_Slice_X;
					uv.Start.y = (float)(pTexture->AnimationNum / pTexture->UV_Slice_X) * 1.0f / (float)pTexture->UV_Slice_Y;

					uv.End.x = (float)uv.Start.x + 1.0f / (float)pTexture->UV_Slice_X;
					uv.End.y = (float)uv.Start.y + 1.0f / (float)pTexture->UV_Slice_Y;
				}
				graphicsContext->SetUVMatrix(uv);
				material.Diffuse.w = pTexture->Material.Diffuse.w * pParticle->Particle[i].LifeTime / pParticle->particleLifeTime;

			} else{
				// マテリアル設定
				material.DiffuseTextureEnable = false;
				material.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

				UVMatrix uv;
				graphicsContext->SetUVMatrix(uv);
			}
			graphicsContext->SetMaterial(material);

			TransformComponent transform = *pTransform;
			transform.position += pParticle->Particle[i].Position * pParticle->particleSize;
			transform.scale *= pParticle->particleSize;

			BillBoardRendererComponent billBoard;
			DirectX::XMMATRIX InvViewBillBoardMatrix = DirectX::XMMatrixRotationQuaternion(transform.rotationVector());
			DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, ctx.viewMatrix);
			DirectX::XMFLOAT4X4 invViewFloat4x4;
			DirectX::XMStoreFloat4x4(&invViewFloat4x4, invView);

			DirectX::XMVECTOR forward = DirectX::XMVectorSet(invViewFloat4x4._31, invViewFloat4x4._32, invViewFloat4x4._33, 0.0f);
			DirectX::XMVECTOR right = DirectX::XMVectorSet(invViewFloat4x4._11, invViewFloat4x4._12, invViewFloat4x4._13, 0.0f);
			DirectX::XMVECTOR up = DirectX::XMVectorSet(invViewFloat4x4._21, invViewFloat4x4._22, invViewFloat4x4._23, 0.0f);

			const DirectX::XMVECTOR worldRight = DirectX::XMVectorSet(1, 0, 0, 0);
			const DirectX::XMVECTOR worldUp = DirectX::XMVectorSet(0, 1, 0, 0);
			const DirectX::XMVECTOR worldForward = DirectX::XMVectorSet(0, 0, 1, 0);

			if(!billBoard.RotateXYZ.x) right = worldRight;
			if(!billBoard.RotateXYZ.y) up = worldUp;
			if(!billBoard.RotateXYZ.z) forward = worldForward;

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

			deviceContext->IASetInputLayout(m_billBoardMesh->mesh.m_VertexLayout.Get());

			deviceContext->VSSetShader(m_billBoardMesh->mesh.m_VertexShader.Get(), NULL, 0);
			deviceContext->PSSetShader(m_billBoardMesh->mesh.m_PixelShader.Get(), NULL, 0);

			// ローカル変換行列（スケール・ビルボード回転・位置）
			DirectX::XMMATRIX LocalMatrix =
				DirectX::XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z) *
				InvViewBillBoardMatrix *
				DirectX::XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);

			DirectX::XMMATRIX WorldMatrix = LocalMatrix;

			graphicsContext->SetWorldMatrix(WorldMatrix);
			UINT stride = sizeof(VERTEX_3D);
			UINT offset = 0;

			deviceContext->IASetVertexBuffers(0, 1, m_billBoardMesh->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);

			deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

			deviceContext->Draw(m_billBoardMesh->mesh.meshCount, 0);
		}
	}
	graphicsContext->SetBlendMode(BlendMode::Alpha);
}
