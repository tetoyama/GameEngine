#include "RenderableWave.h"

#include <d3d11.h>
#include "../../RenderPass/RenderPassContext.h"

#include "DebugTools/DebugSystem.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/componentRegistry.h"

#include "Scene/Component/waveComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/textureComponent.h"
#include <Component/materialComponent.h>

#include "Resources/resourceService.h"
#include "Resources/Data/shaderData.h"

// WaveGBufferPS.hlsl のパス (プロジェクトルートからの相対パス)
static constexpr const char* WAVE_GBUFFER_PS_PATH = "Source/Shader/WaveGBufferPS.hlsl";

void RenderableWave::Initialize(SceneManagerContext* context) {
	m_context = context;
	// 水面専用 GBuffer ピクセルシェーダをランタイムコンパイルで読み込む
	m_wavePS = context->resource->Load<PixelShaderData>(WAVE_GBUFFER_PS_PATH);
	if (!m_wavePS) {
		OutputDebugStringA("RenderableWave: WaveGBufferPS.hlsl の読み込みに失敗しました。標準 GBufferPS を使用します。\n");
	}
}

void RenderableWave::Finalize() {
	m_wavePS.reset();
}

void RenderableWave::Execute(const RenderPassContext& ctx, SceneContext* sceneContext, const Entity& entity) {
	auto componentRegistry = sceneContext->component;
	auto pTransform = componentRegistry->GetComponent<TransformComponent>(entity);
	auto pWave = componentRegistry->GetComponent<WaveComponent>(entity);

	if (!pWave || !pWave->meshRenderer) {
		return;
	}

	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	// ---- 水面専用 GBuffer PS に切り替え ----
	ID3D11PixelShader* prevPS = nullptr;
	bool usingWavePS = (m_wavePS && m_wavePS->m_PixelShader);
	if (usingWavePS) {
		deviceContext->PSGetShader(&prevPS, nullptr, nullptr);
		deviceContext->PSSetShader(m_wavePS->m_PixelShader.Get(), nullptr, 0);
	}

	// ---- テクスチャ & マテリアル ----
	TextureComponent* pTexture = componentRegistry->GetComponent<TextureComponent>(entity);
	MaterialComponent* pMaterial = componentRegistry->GetComponent<MaterialComponent>(entity);
	MATERIAL material{};
	if (pMaterial) {
		material = pMaterial->Material;
	}

	if(pTexture && pTexture->m_TextureData && pTexture->m_TextureData->pTexture){
		deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
		material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
	} else {
		// テクスチャ未設定の場合は水色ベースカラー
		material.BaseColor = DirectX::XMFLOAT4(0.15f, 0.45f, 0.85f, 1.0f);
	}

	// 水面マテリアル値を上書き (WaveComponent の値を使用)
	material.Metallic   = pWave->WaterMetallic;
	material.Roughness  = pWave->WaterRoughness;
	material.AO         = 1.0f;
	graphicsContext->SetMaterial(material);

	// ---- WaveGBufferPS 用 Parameter (NormalIntensity, Roughness, Metallic, Time) ----
	if (usingWavePS) {
		DirectX::XMFLOAT4 param(
			pWave->NormalIntensity,
			pWave->WaterRoughness,
			pWave->WaterMetallic,
			pWave->Time
		);
		graphicsContext->SetParameter(param);
	}

	// ---- ジオメトリ設定 & 描画 ----
	auto meshRenderer = pWave->meshRenderer;

	DirectX::XMMATRIX World = pTransform->CalculateWorldMatrix(pTransform, componentRegistry);
	graphicsContext->SetWorldMatrix(World);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetIndexBuffer(*meshRenderer->mesh.m_IndexBuffer.GetAddressOf(), DXGI_FORMAT_R32_UINT, 0);
	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0, 0);

	// ---- GBuffer PS を元に戻す ----
	if (usingWavePS) {
		if (prevPS) {
			deviceContext->PSSetShader(prevPS, nullptr, 0);
		}
	}
	if (prevPS) {
		prevPS->Release();
	}

	graphicsContext->SetDepthMode(DepthMode::Write);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}

