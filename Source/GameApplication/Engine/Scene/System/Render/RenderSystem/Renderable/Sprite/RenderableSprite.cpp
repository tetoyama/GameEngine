// =======================================================================
// 
// RenderableSprite.cpp
// 
// =======================================================================
#include "RenderableSprite.h"

#include <d3d11.h>
#include "../../RenderPass/RenderPassContext.h"

#include "DebugTools/DebugSystem.h"
#include "Graphics/mainRenderer.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/componentRegistry.h"

#include "Scene/Component/2DspriteRendererComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/textureComponent.h"
#include <Component/materialComponent.h>

void RenderableSprite::Initialize(SceneManagerContext* context){
	m_spriteMesh = new MeshRendererComponent;
	if(m_spriteMesh){

		m_spriteMesh->mesh.meshCount = 4;
		VERTEX_3D m_Vertex[4]{};

		vertex[0].Position = DirectX::XMFLOAT3(0.5f, 0.5f, 0.0f);
		vertex[0].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[0].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[0].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[0].TexCoord = DirectX::XMFLOAT2(1.0f, 1.0f);

		vertex[1].Position = DirectX::XMFLOAT3(-0.5f, 0.5f, 0.0f);
		vertex[1].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[1].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[1].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[1].TexCoord = DirectX::XMFLOAT2(0.0f, 1.0f);

		vertex[2].Position = DirectX::XMFLOAT3(0.5f, -0.5f, 0.0f);
		vertex[2].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[2].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[2].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[2].TexCoord = DirectX::XMFLOAT2(1.0f, 0.0f);

		vertex[3].Position = DirectX::XMFLOAT3(-0.5f, -0.5f, 0.0f);
		vertex[3].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[3].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[3].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[3].TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);

		D3D11_BUFFER_DESC m_Bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(VERTEX_3D) * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA m_Sd{};
		sd.pSysMem = vertex;

		context->renderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, m_spriteMesh->mesh.m_VertexBuffer.GetAddressOf());
		context->renderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\commonVS.cso", m_spriteMesh->mesh.m_VertexShader.GetAddressOf(), m_spriteMesh->mesh.m_VertexLayout.GetAddressOf());
		context->renderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\unlitUVTexturePS.cso", m_spriteMesh->mesh.m_PixelShader.GetAddressOf());
	}
}

void RenderableSprite::Finalize(){
	delete m_SpriteMesh;
}

void RenderableSprite::Execute(const RenderPassContext& ctx, SceneContext* sceneContext, const Entity& entity){

	SpriteRendererComponent* spriteRenderer = sceneContext->component->GetComponent<SpriteRendererComponent>(entity);
	TransformComponent* transform = sceneContext->component->GetComponent<TransformComponent>(entity);
	if(!spriteRenderer || !transform){
		return;
	}



	Vector2 m_ViewportSize= Vector2(
		(float)sceneContext->manager->graphics->m_width,
		(float)sceneContext->manager->graphics->m_height
	);

	TransformComponent m_NewTransform= transform->CalculateRectTransform(viewportSize, *spriteRenderer, *transform);

	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11Device* device = graphicsContext->GetDevice();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	ComponentRegistry* componentRegistry = sceneContext->component;


	MATERIAL m_Material{};
	material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	MaterialComponent* pMaterial = sceneContext->component->GetComponent<MaterialComponent>(entity);
	if (pMaterial) {
		material = pMaterial->Material;
	}

	TextureComponent* pTexture = sceneContext->component->GetComponent<TextureComponent>(entity);
	if (pTexture) {

			// マテリアル設定
		if (pTexture->m_TextureData) {
			material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
			deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
		}

		graphicsContext->SetMaterial(material);

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
		// マテリアル設定
		MATERIAL m_Material{};
		material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		graphicsContext->SetMaterial(material);

		UVMatrixBuffer m_Uv;
		graphicsContext->SetUVMatrixBuffer(uv);

	}
	if(m_spriteMesh->mesh.m_VertexLayout){
		deviceContext->IASetInputLayout(m_spriteMesh->mesh.m_VertexLayout.Get());
	}
	if(m_spriteMesh->mesh.m_VertexShader){
		deviceContext->VSSetShader(m_spriteMesh->mesh.m_VertexShader.Get(), NULL, 0);
	}
	if(m_spriteMesh->mesh.m_PixelShader){
		deviceContext->PSSetShader(m_spriteMesh->mesh.m_PixelShader.Get(), NULL, 0);
	}
	DirectX::XMMATRIX m_World= newTransform.CalculateWorldMatrix(&newTransform, componentRegistry);

	graphicsContext->SetWorldViewProjection2D();
	graphicsContext->SetWorldMatrix(World);
	UINT m_Stride= sizeof(VERTEX_3D);
	UINT m_Offset= 0;

	deviceContext->IASetVertexBuffers(0, 1, m_spriteMesh->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//if (ctx.passPhase == RenderPhase::PHASE_SHADOW) {
	//	deviceContext->PSSetShader(nullptr, NULL, 0); // ピクセルシェーダー無効化
	//}
	deviceContext->Draw(m_spriteMesh->mesh.meshCount, 0);

	graphicsContext->SetDepthMode(DepthMode::Write);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}
