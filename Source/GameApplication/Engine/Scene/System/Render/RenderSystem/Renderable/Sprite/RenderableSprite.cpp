// =======================================================================
// 
// RenderableSprite.cpp
// 
// =======================================================================
#include "RenderableSprite.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
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

	// インスタンシング用バッファとシェーダー
	ID3D11Device* device = context->renderer->GetGraphicsContext()->GetDevice();
	if (device) {
		D3D11_BUFFER_DESC instDesc{};
		instDesc.Usage = D3D11_USAGE_DYNAMIC;
		instDesc.ByteWidth = sizeof(DirectX::XMFLOAT4) * 4;
		instDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		instDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		device->CreateBuffer(&instDesc, nullptr, m_instanceBuffer.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> vsErr;
		HRESULT hr = D3DCompileFromFile(
			L"Source\\Shader\\SpriteInstanceVS.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"main",
			"vs_5_0",
			D3DCOMPILE_ENABLE_STRICTNESS,
			0,
			vsBlob.GetAddressOf(),
			vsErr.GetAddressOf());
		if (SUCCEEDED(hr)) {
			device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_instanceVS.GetAddressOf());

			D3D11_INPUT_ELEMENT_DESC layout[] =
			{
				{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 4 * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TANGENT",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 4 * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "COLOR",		0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, 4 * 9, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0, 4 * 13, D3D11_INPUT_PER_VERTEX_DATA, 0 },

				{ "TEXCOORD",	1, DXGI_FORMAT_R32G32B32A32_FLOAT,	1, 0,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
				{ "TEXCOORD",	2, DXGI_FORMAT_R32G32B32A32_FLOAT,	1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
				{ "TEXCOORD",	3, DXGI_FORMAT_R32G32B32A32_FLOAT,	1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
				{ "TEXCOORD",	4, DXGI_FORMAT_R32G32B32A32_FLOAT,	1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			};
			device->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), m_instanceLayout.GetAddressOf());
		}
	}
}

void RenderableSprite::Finalize(){
	delete m_spriteMesh;
}

void RenderableSprite::Execute(const RenderPassContext& ctx, SceneContext* sceneContext, const Entity& entity){

	SpriteRendererComponent* spriteRenderer = sceneContext->component->GetComponent<SpriteRendererComponent>(entity);
	TransformComponent* transform = sceneContext->component->GetComponent<TransformComponent>(entity);
	if(!spriteRenderer || !transform){
		return;
	}



	Vector2 viewportSize = Vector2(
		(float)sceneContext->manager->graphics->m_width,
		(float)sceneContext->manager->graphics->m_height
	);

	TransformComponent newTransform = transform->CalculateRectTransform(viewportSize, *spriteRenderer, *transform);

	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11Device* device = graphicsContext->GetDevice();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	ComponentRegistry* componentRegistry = sceneContext->component;


	MATERIAL material{};
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

		UVMatrixBuffer uv;
		if (pTexture->UV_Slice_X != 0 && pTexture->UV_Slice_Y != 0) {
			uv.UVStart.x = (float)(pTexture->AnimationNum % pTexture->UV_Slice_X) * 1.0f / (float)pTexture->UV_Slice_X;
			uv.UVStart.y = (float)(pTexture->AnimationNum / pTexture->UV_Slice_X) * 1.0f / (float)pTexture->UV_Slice_Y;

			uv.UVEnd.x = (float)uv.UVStart.x + 1.0f / (float)pTexture->UV_Slice_X;
			uv.UVEnd.y = (float)uv.UVStart.y + 1.0f / (float)pTexture->UV_Slice_Y;
		}
		graphicsContext->SetUVMatrixBuffer(uv);

	} else {
		// マテリアル設定
		MATERIAL material{};
		material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		graphicsContext->SetMaterial(material);

		UVMatrixBuffer uv;
		graphicsContext->SetUVMatrixBuffer(uv);

	}
	if(m_spriteMesh->mesh.m_VertexLayout){
		if (m_instanceLayout) {
			deviceContext->IASetInputLayout(m_instanceLayout.Get());
		} else {
			deviceContext->IASetInputLayout(m_spriteMesh->mesh.m_VertexLayout.Get());
		}
	}
	if(m_instanceVS){
		deviceContext->VSSetShader(m_instanceVS.Get(), NULL, 0);
	} else if(m_spriteMesh->mesh.m_VertexShader){
		deviceContext->VSSetShader(m_spriteMesh->mesh.m_VertexShader.Get(), NULL, 0);
	}
	if(m_spriteMesh->mesh.m_PixelShader){
		deviceContext->PSSetShader(m_spriteMesh->mesh.m_PixelShader.Get(), NULL, 0);
	}
	DirectX::XMMATRIX World = newTransform.CalculateWorldMatrix(&newTransform, componentRegistry);

	graphicsContext->SetWorldViewProjection2D();
	graphicsContext->SetWorldMatrix(World);
	UINT stride[2] = { sizeof(VERTEX_3D), sizeof(DirectX::XMFLOAT4) * 4 };
	UINT offset[2] = { 0, 0 };

	ID3D11Buffer* buffers[2] = { m_spriteMesh->mesh.m_VertexBuffer.Get(), m_instanceBuffer.Get() };
	deviceContext->IASetVertexBuffers(0, 2, buffers, stride, offset);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//if (ctx.passPhase == RenderPhase::PHASE_SHADOW) {
	//	deviceContext->PSSetShader(nullptr, NULL, 0); // ピクセルシェーダー無効化
	//}
	if (m_instanceBuffer && m_instanceVS) {
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (SUCCEEDED(deviceContext->Map(m_instanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
			DirectX::XMFLOAT4X4 mtx;
			DirectX::XMStoreFloat4x4(&mtx, DirectX::XMMatrixTranspose(World));
			memcpy(mapped.pData, &mtx, sizeof(DirectX::XMFLOAT4) * 4);
			deviceContext->Unmap(m_instanceBuffer.Get(), 0);
		}
		deviceContext->DrawInstanced(m_spriteMesh->mesh.meshCount, 1, 0, 0);
	} else {
		deviceContext->Draw(m_spriteMesh->mesh.meshCount, 0);
	}

	graphicsContext->SetDepthMode(DepthMode::Write);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}
