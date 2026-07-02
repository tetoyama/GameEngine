// =======================================================================
// 
// RenderableTerrain.cpp
// 
// =======================================================================
#include "RenderableTerrain.h"

#include <d3d11.h>
#include <algorithm>
#include "../../RenderPass/RenderPassContext.h"
#include "../../RenderPacket/RenderPacketTransformDX11.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Component/terrainComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/textureComponent.h"
#include <Component/materialComponent.h>

void RenderableTerrain::Execute(const RenderPassContext& ctx, const RenderPacket& packet){
	SceneContext* sceneContext = packet.bindings.sceneContext;
	if(!sceneContext) return;

	TerrainComponent* terrain = packet.bindings.terrain;
	TransformComponent* transform = packet.bindings.transform;
	if(!terrain || !terrain->meshRenderer || !transform) return;

	auto meshRenderer = terrain->meshRenderer;
	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	MATERIAL material{};
	material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	if(MaterialComponent* source = packet.bindings.material){
		material = source->Material;
	}

	UVMatrixBuffer uv{};
	uv.UVStart = float2(0.0f, 0.0f);
	uv.UVEnd = float2(1.0f, 1.0f);
	if(TextureComponent* texture = packet.bindings.texture){
		if(texture->m_TextureData){
			material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
			deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, texture->m_TextureData->pTexture.GetAddressOf());
		}
		uv = texture->ResolveUVMatrixBuffer();
	}
	graphicsContext->SetMaterial(material);
	graphicsContext->SetUVMatrixBuffer(uv);

	const DirectX::XMMATRIX world = LoadRenderPacketMatrix(packet.transform.worldMatrix);
	graphicsContext->SetWorldMatrix(world);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetIndexBuffer(*meshRenderer->mesh.m_IndexBuffer.GetAddressOf(), DXGI_FORMAT_R32_UINT, 0);
	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0, 0);

	graphicsContext->SetDepthMode(DepthMode::Write);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}