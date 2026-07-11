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

	MeshRendererComponent* meshRenderer = wave->meshRenderer;
	ID3D11Buffer* vertexBuffer = meshRenderer->mesh.m_VertexBuffer.Get();
	ID3D11Buffer* indexBuffer = meshRenderer->mesh.m_IndexBuffer.Get();
	if(!vertexBuffer || !indexBuffer || meshRenderer->mesh.indexCount <= 0){
		return;
	}

	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	if(!graphicsContext) return;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	if(!deviceContext) return;

	MATERIAL material{};
	material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	if(MaterialComponent* source = packet.bindings.material){
		material = source->Material;
		material.MaterialFlags &= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
	}

	UVMatrixBuffer uv{};
	uv.UVStart = float2(0.0f, 0.0f);
	uv.UVEnd = float2(1.0f, 1.0f);
	if(TextureComponent* texture = packet.bindings.texture){
		uv = texture->ResolveUVMatrixBuffer();
		if(texture->m_TextureData && texture->m_TextureData->pTexture){
			deviceContext->PSSetShaderResources(
				TextureSlot_Albedo,
				1,
				texture->m_TextureData->pTexture.GetAddressOf()
			);
			material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
		}
	}

	const DirectX::XMMATRIX world =
		LoadRenderPacketMatrix(packet.transform.worldMatrix);
	graphicsContext->SetCullMode(CullMode::Back);
	graphicsContext->SetPerObjectConstants(world, material, uv);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	);
	deviceContext->IASetVertexBuffers(
		0,
		1,
		&vertexBuffer,
		&stride,
		&offset
	);
	deviceContext->IASetIndexBuffer(
		indexBuffer,
		DXGI_FORMAT_R32_UINT,
		0
	);
	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0, 0);
}
