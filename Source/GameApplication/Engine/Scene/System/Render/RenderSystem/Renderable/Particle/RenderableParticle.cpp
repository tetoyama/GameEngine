// =======================================================================
// 
// RenderableParticle.cpp
// 
// =======================================================================
#include "RenderableParticle.h"
#include <d3d11.h>
#include "../../RenderPass/RenderPassContext.h"

#include "DebugTools/DebugSystem.h"
#include "Graphics/mainRenderer.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/componentRegistry.h"

#include "Scene/Component/particleComponent.h"
#include "Scene/Component/BillBoardRendererComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/textureComponent.h"
#include <Component/materialComponent.h>
void RenderableParticle::Initialize(SceneManagerContext* context){
	m_pBillBoardMesh = new MeshRendererComponent;
	if(m_pBillBoardMesh){

		m_pBillBoardMesh->mesh.meshCount = 4;
		VERTEX_3D m_Vertex[4]{};

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

		D3D11_BUFFER_DESC m_Bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(VERTEX_3D) * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA m_Sd{};
		sd.pSysMem = vertex;

		context->renderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, m_pBillBoardMesh->mesh.m_VertexBuffer.GetAddressOf());
		context->renderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\commonVS.cso", m_pBillBoardMesh->mesh.m_VertexShader.GetAddressOf(), m_pBillBoardMesh->mesh.m_VertexLayout.GetAddressOf());
		context->renderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\unlitUVTexturePS.cso", m_pBillBoardMesh->mesh.m_PixelShader.GetAddressOf());
	}
}

void RenderableParticle::Finalize(){
	delete m_BillBoardMesh;
}

void RenderableParticle::Execute(const RenderPassContext& ctx, SceneContext* sceneContext, const Entity& entity){
	ParticleComponent* pParticle = sceneContext->component->GetComponent<ParticleComponent>(entity);
	TransformComponent* pTransform = sceneContext->component->GetComponent<TransformComponent>(entity);
	if(!pParticle || !pTransform){
		return;
	}
	TextureComponent* pTexture = sceneContext->component->GetComponent<TextureComponent>(entity);
	MaterialComponent* pMaterial = sceneContext->component->GetComponent<MaterialComponent>(entity);
	MATERIAL m_Material{};
	if (pMaterial) {
		material = pMaterial->Material;
	}

	GraphicsContext* graphicsContext = sceneContext->manager->renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	graphicsContext->SetBlendMode(BlendMode::Additive);
	//graphicsContext->SetDepthMode(DepthMode::Disable);

	for(int i = 0; i < MAXPARTICLE; i++){
		if(pParticle->Particle[i].LifeTime > 0.0f){
			MATERIAL m_Material{};

			if(pTexture){
				// マテリアル設定
				if(pTexture->m_TextureData){
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

				if (pMaterial) {
					material.BaseColor = pMaterial->Material.BaseColor;
					material.BaseColor.w = pMaterial->Material.BaseColor.w * pParticle->Particle[i].LifeTime / pParticle->particleLifeTime;
				}
			} else{
				// マテリアル設定
				material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

				UVMatrixBuffer m_Uv;
				graphicsContext->SetUVMatrixBuffer(uv);
			}
			graphicsContext->SetMaterial(material);

			TransformComponent m_Transform= *pTransform;
			transform.position += pParticle->Particle[i].Position * pParticle->particleSize;
			transform.scale *= pParticle->particleSize;

			BillBoardRendererComponent m_BillBoard;
			DirectX::XMMATRIX m_InvViewBillBoardMatrix= DirectX::XMMatrixRotationQuaternion(transform.rotationVector());
			DirectX::XMMATRIX m_InvView= DirectX::XMMatrixInverse(nullptr, ctx.viewMatrix);
			DirectX::XMFLOAT4X4 m_InvViewFloat4x4;
			DirectX::XMStoreFloat4x4(&invViewFloat4x4, invView);

			DirectX::XMVECTOR m_Forward= DirectX::XMVectorSet(invViewFloat4x4._31, invViewFloat4x4._32, invViewFloat4x4._33, 0.0f);
			DirectX::XMVECTOR m_Right= DirectX::XMVectorSet(invViewFloat4x4._11, invViewFloat4x4._12, invViewFloat4x4._13, 0.0f);
			DirectX::XMVECTOR m_Up= DirectX::XMVectorSet(invViewFloat4x4._21, invViewFloat4x4._22, invViewFloat4x4._23, 0.0f);

			const DirectX::XMVECTOR m_WorldRight= DirectX::XMVectorSet(1, 0, 0, 0);
			const DirectX::XMVECTOR m_WorldUp= DirectX::XMVectorSet(0, 1, 0, 0);
			const DirectX::XMVECTOR m_WorldForward= DirectX::XMVectorSet(0, 0, 1, 0);

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

			//deviceContext->IASetInputLayout(m_pBillBoardMesh->mesh.m_VertexLayout.Get());

			//deviceContext->VSSetShader(m_pBillBoardMesh->mesh.m_VertexShader.Get(), NULL, 0);
			//deviceContext->PSSetShader(m_pBillBoardMesh->mesh.m_PixelShader.Get(), NULL, 0);

			// ローカル変換行列（スケール・ビルボード回転・位置）
			DirectX::XMMATRIX m_LocalMatrix=
				DirectX::XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z) *
				InvViewBillBoardMatrix *
				DirectX::XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);

			DirectX::XMMATRIX m_WorldMatrix= LocalMatrix;

			graphicsContext->SetWorldMatrix(WorldMatrix);
			UINT m_Stride= sizeof(VERTEX_3D);
			UINT m_Offset= 0;

			deviceContext->IASetVertexBuffers(0, 1, m_pBillBoardMesh->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);

			deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			//if (ctx.passPhase == RenderPhase::PHASE_SHADOW) {
			//	deviceContext->PSSetShader(nullptr, NULL, 0); // ピクセルシェーダー無効化
			//}
			deviceContext->Draw(m_pBillBoardMesh->mesh.meshCount, 0);
		}
	}
	graphicsContext->SetBlendMode(BlendMode::Alpha);
	//graphicsContext->SetDepthMode(DepthMode::Write);
}
