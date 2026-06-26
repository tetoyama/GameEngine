// =======================================================================
// 
// RenderableWave.cpp
// 
// =======================================================================
#include "RenderableWave.h"

#include <d3d11.h>
#include "../../RenderPass/RenderPassContext.h"
#include "../../RenderPacket/RenderPacketTransformDX11.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Component/waveComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/textureComponent.h"
#include <Component/materialComponent.h>
#include "Shader/commonDefine.h"

void RenderableWave::Execute(const RenderPassContext& ctx, const RenderPacket& packet){
	(void)ctx;
	SceneContext* sceneContext = packet.bindings.sceneContext;
	WaveComponent* wave = packet.bindings.wave;
	if(!sceneContext || !wave || !wave->meshRenderer) return;

	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	MATERIAL material{};
	material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	if(MaterialComponent* source = packet.bindings.material){
		material = source->Material;
		material.MaterialFlags &= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
	}

	if(TextureComponent* texture = packet.bindings.texture;
		texture && texture->m_TextureData && texture->m_TextureData->pTexture){
		deviceContext->PSSetShaderResources(
			TextureSlot_Albedo,
			1,
			texture->m_TextureData->pTexture.GetAddressOf()
		);
		material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
	}
	graphicsContext->SetMaterial(material);

	auto meshRenderer = wave->meshRenderer;
	const DirectX::XMMATRIX world =
		LoadRenderPacketMatrix(packet.transform.worldMatrix);
	graphicsContext->SetCullMode(CullMode::Back);
	graphicsContext->SetWorldMatrix(world);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	);
	deviceContext->IASetVertexBuffers(
		0,
		1,
		meshRenderer->mesh.m_VertexBuffer.GetAddressOf(),
		&stride,
		&offset
	);
	deviceContext->IASetIndexBuffer(
		*meshRenderer->mesh.m_IndexBuffer.GetAddressOf(),
		DXGI_FORMAT_R32_UINT,
		0
	);
	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0, 0);
}
