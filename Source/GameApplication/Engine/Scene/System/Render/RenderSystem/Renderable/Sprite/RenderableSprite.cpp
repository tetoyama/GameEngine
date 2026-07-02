// =======================================================================
// 
// RenderableSprite.cpp
// 
// =======================================================================
#include "RenderableSprite.h"

#include <d3d11.h>
#include <algorithm>
#include "../../RenderPass/RenderPassContext.h"
#include "../../RenderPacket/RenderPacketTransformDX11.h"

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
		VERTEX_3D vertex[4]{};

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

		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(VERTEX_3D) * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = vertex;

		context->renderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, m_spriteMesh->mesh.m_VertexBuffer.GetAddressOf());
		context->renderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\commonVS.cso", m_spriteMesh->mesh.m_VertexShader.GetAddressOf(), m_spriteMesh->mesh.m_VertexLayout.GetAddressOf());
		context->renderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\unlitUVTexturePS.cso", m_spriteMesh->mesh.m_PixelShader.GetAddressOf());
	}
}

void RenderableSprite::Finalize(){
	delete m_spriteMesh;
}

void RenderableSprite::Execute(const RenderPassContext& ctx, const RenderPacket& packet){
	SceneContext* sceneContext = packet.bindings.sceneContext;
	if(!sceneContext) return;

	SpriteRendererComponent* spriteRenderer = packet.bindings.spriteRenderer;
	TransformComponent* transform = packet.bindings.transform;
	if(!spriteRenderer || !transform){
		return;
	}

	const Vector2 viewportSize = ctx.screenSize;
	TransformComponent newTransform = transform->CalculateRectTransform(viewportSize, *spriteRenderer, *transform);

	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	MATERIAL material{};
	material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	MaterialComponent* pMaterial = packet.bindings.material;
	if(pMaterial){
		material = pMaterial->Material;
	}

	TextureComponent* pTexture = packet.bindings.texture;
	if(pTexture){
		if(pTexture->m_TextureData){
			material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
			ID3D11ShaderResourceView* textureSRV = pTexture->m_TextureData->pTexture.Get();
			deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, &textureSRV);
		}

		graphicsContext->SetMaterial(material);
		graphicsContext->SetUVMatrixBuffer(pTexture->ResolveUVMatrixBuffer());
	}else{
		graphicsContext->SetMaterial(material);
		UVMatrixBuffer uv{};
		uv.UVStart = float2(0.0f, 0.0f);
		uv.UVEnd = float2(1.0f, 1.0f);
		graphicsContext->SetUVMatrixBuffer(uv);
	}

	if(m_spriteMesh->mesh.m_VertexLayout){
		deviceContext->IASetInputLayout(m_spriteMesh->mesh.m_VertexLayout.Get());
	}
	if(m_spriteMesh->mesh.m_VertexShader){
		deviceContext->VSSetShader(m_spriteMesh->mesh.m_VertexShader.Get(), nullptr, 0);
	}
	if(m_spriteMesh->mesh.m_PixelShader){
		deviceContext->PSSetShader(m_spriteMesh->mesh.m_PixelShader.Get(), nullptr, 0);
	}

	DirectX::XMMATRIX world = TransformMath::CalculateLocalMatrix(newTransform);
	if(packet.transform.hasParentWorld){
		world = world * LoadRenderPacketMatrix(packet.transform.parentWorldMatrix);
	}

	// 2D ProjectionはSwapChainサイズではなく、現在描画しているRender Targetのサイズを使う。
	// Player View / Editor Viewの縮小Render TargetでもRectが同じ正規化契約で配置される。
	graphicsContext->SetViewMatrix(DirectX::XMMatrixIdentity());
	graphicsContext->SetProjectionMatrix(
		DirectX::XMMatrixOrthographicOffCenterLH(
			0.0f,
			viewportSize.x,
			viewportSize.y,
			0.0f,
			0.0f,
			1.0f
		)
	);
	graphicsContext->SetDepthMode(DepthMode::Disable);
	graphicsContext->SetWorldMatrix(world);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, m_spriteMesh->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	deviceContext->Draw(m_spriteMesh->mesh.meshCount, 0);

	graphicsContext->SetDepthMode(DepthMode::Write);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}