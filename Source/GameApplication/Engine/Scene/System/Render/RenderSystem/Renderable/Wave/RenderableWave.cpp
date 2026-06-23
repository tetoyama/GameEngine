// =======================================================================
// 
// RenderableWave.cpp
// 
// =======================================================================
#include "RenderableWave.h"

#include <d3d11.h>
#include "../../RenderPass/RenderPassContext.h"
#include "../../RenderPacket/RenderPacketTransformDX11.h"

#include "DebugTools/DebugSystem.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/componentRegistry.h"

#include "Scene/Component/waveComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/textureComponent.h"
#include <Component/materialComponent.h>
#include "Shader/commonDefine.h"


void RenderableWave::Execute(const RenderPassContext& ctx, const RenderPacket& packet){
	SceneContext* sceneContext = packet.bindings.sceneContext;
	const Entity& entity = packet.entity;
	if(!sceneContext) return;
	auto pTransform = packet.bindings.transform;
	auto pWave = packet.bindings.wave;

	if (!pWave || !pWave->meshRenderer) {
		return;
	}
	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11Device* device = graphicsContext->GetDevice();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	TextureComponent* pTexture = packet.bindings.texture;
	MaterialComponent* pMaterial = packet.bindings.material;
	MATERIAL material;
	if (pMaterial) {
		material = pMaterial->Material;
		material.MaterialFlags &= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
	}

	if(pTexture && pTexture->m_TextureData && pTexture->m_TextureData->pTexture){
		deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());

		// sceneContext->manager->graphics->GetDeviceContext()->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
		material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;

	}

	sceneContext->manager->graphics->SetMaterial(material);

	auto meshRenderer = pWave->meshRenderer;
	auto transform = pTransform;


	DirectX::XMMATRIX World =
		LoadRenderPacketMatrix(packet.transform.worldMatrix);

	graphicsContext->SetCullMode(CullMode::Back);
	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetIndexBuffer(*meshRenderer->mesh.m_IndexBuffer.GetAddressOf(), DXGI_FORMAT_R32_UINT, 0);

	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0, 0);

	//graphicsContext->SetDepthMode(DepthMode::Write);
	//graphicsContext->SetViewMatrix(ctx.viewMatrix);
	//graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}
