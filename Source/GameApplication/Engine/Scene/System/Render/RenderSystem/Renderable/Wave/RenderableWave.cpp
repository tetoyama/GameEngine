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
#include "Graphics/graphicsContext.h"
#include "Shader/commonDefine.h"


void RenderableWave::Execute(const RenderPassContext& ctx, SceneContext* sceneContext, const Entity& entity) {
	auto componentRegistry = sceneContext->component;
	auto pTransform = componentRegistry->GetComponent<TransformComponent>(entity);
	auto pWave = componentRegistry->GetComponent<WaveComponent>(entity);

	if (!pWave || !pWave->meshRenderer) {
		return;
	}

	TextureComponent* pTexture = sceneContext->component->GetComponent<TextureComponent>(entity);
	MaterialComponent* pMaterial = sceneContext->component->GetComponent<MaterialComponent>(entity);
	MATERIAL material{};
	if (pMaterial) {
		material = pMaterial->Material;
	}

	if(pTexture && pTexture->m_TextureData && pTexture->m_TextureData->pTexture){

		sceneContext->manager->graphics->GetDeviceContext()->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
		material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;

	} else{

		material.BaseColor = DirectX::XMFLOAT4(1, 1, 1, 1);
	}

	// 水面反射が有効な場合、反射テクスチャをバインドしてマテリアルフラグをセット
	if (pWave->ReflectionEnabled && reflectionSRV) {
		sceneContext->manager->graphics->GetDeviceContext()->PSSetShaderResources(
			TextureSlot_Reflection, 1, &reflectionSRV
		);
		material.MaterialFlags |= MATERIAL_FLAG_USE_WATER_REFLECTION;
		// Roughness に反射強度を保持（1.0 = 最大反射、0.0 = 反射なし）
		material.Roughness      = pWave->ReflectionStrength;
		// AO に歪み強度を保持
		material.AO             = pWave->DistortionStrength;
	}

	// 鏡面ハイライトパラメータを設定
	// Metallic に鏡面ハイライト強度、Pad0 に鋭さを格納（水シェーダー専用）
	material.Metallic = pWave->SpecularStrength;
	material.Pad0     = pWave->SpecularShininess;

	sceneContext->manager->graphics->SetMaterial(material);

	// マテリアルコンポーネントが未設定の場合、反射有効時は水面シェーダー(ID=5)を自動適用
	if (pWave->ReflectionEnabled && reflectionSRV && !pMaterial) {
		ObjectInfo waterInfo;
		waterInfo.SceneID  = (unsigned int)sceneContext;
		waterInfo.ObjectID = entity;
		waterInfo.ShaderID = WATER_SHADER_ID_DEFAULT; // WaterShader (EngineConfig ShaderMaterial 5番目)
		sceneContext->manager->graphics->SetObjectInfo(waterInfo);
	}

	auto meshRenderer = pWave->meshRenderer;
	auto transform = pTransform;

	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, componentRegistry);

	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetIndexBuffer(*meshRenderer->mesh.m_IndexBuffer.GetAddressOf(), DXGI_FORMAT_R32_UINT, 0);

	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0, 0);

	// 反射テクスチャスロットを解除
	if (pWave->ReflectionEnabled && reflectionSRV) {
		ID3D11ShaderResourceView* nullSRV = nullptr;
		deviceContext->PSSetShaderResources(TextureSlot_Reflection, 1, &nullSRV);
	}

	graphicsContext->SetDepthMode(DepthMode::Write);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}
