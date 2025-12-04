#include "ShadowMapPass.h"
#include "System/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderPhase.h"

#include "scene.h"
#include "System/RenderSystem/Renderable/IRenderable.h"
#include "System/RenderSystem/RenderTarget/renderTarget.h"
#include "Component/transformComponent.h"
#include "Registry/componentRegistry.h"

#include "Shader/commonDefine.h"

#include "GameApplication/Engine/Graphics/graphicsContext.h"
#include "Registry/systemRegistry.h"
#include "System/RenderSystem/Renderable/Model/RenderableModel.h"
#include <Component/LightComponent.h>
#include <System/RenderSystem/Renderable/Terrain/RenderableTerrain.h>

void ShadowMapPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {
	m_renderSystem = renderSystem;
	m_context = context;

	D3D11_SAMPLER_DESC desc = {};
	desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;

	context->graphics->GetDevice()->CreateSamplerState(&desc, &shadowSampler);

	shadowRenderTarget = new RenderTarget(
		Vector2(SHADOWMAP_SIZE, SHADOWMAP_SIZE),
		m_context->graphics,
		RenderTargetType::RENDERTARGET_TYPE_DEPTH
	);

	renderables.clear();
	renderables.push_back(renderSystem->GetRenderable<RenderableModel>());
	renderables.push_back(renderSystem->GetRenderable<RenderableTerrain>());
}

void ShadowMapPass::Finalize() {

	shadowSampler->Release();
	shadowSampler = nullptr;

	delete shadowRenderTarget;
	shadowRenderTarget = nullptr;
}

void ShadowMapPass::Execute(const RenderPassContext& ctx) {

	ID3D11DeviceContext* deviceContext = m_context->graphics->GetDeviceContext();

	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	deviceContext->PSSetShaderResources(2, 1, nullSRV);
	deviceContext->PSSetShader(nullptr, NULL, 0); // ピクセルシェーダー無効化

	// シャドウマップレンダーターゲットに切り替え
	if (shadowRenderTarget->type == RenderTargetType::RENDERTARGET_TYPE_DEPTH) {

		deviceContext->ClearDepthStencilView(shadowRenderTarget->dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		deviceContext->OMSetRenderTargets(0, nullptr, shadowRenderTarget->dsv.Get());

	} else {
		float color[4] = { 1.0f,1.0f,1.0f,1.0f };
		deviceContext->ClearRenderTargetView(shadowRenderTarget->rtv.Get(), color);
		deviceContext->ClearDepthStencilView(shadowRenderTarget->dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		deviceContext->OMSetRenderTargets(1, shadowRenderTarget->rtv.GetAddressOf(), shadowRenderTarget->dsv.Get());
	}

	RenderPassContext newContext = ctx;
	newContext.passPhase = RenderPhase::PHASE_SHADOW;
	newContext.screenSize = Vector2(SHADOWMAP_SIZE, SHADOWMAP_SIZE);

	// シャドウマップ用カメラ
	LIGHT light = m_context->renderer->GetGraphicsContext()->GetLight()[0];
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		// ライトコンポーネントを持つエンティティの検索
		const auto& lightEntities = context->component->FindEntitiesWithComponent<LightComponent>();
		if (lightEntities.empty()) {
			continue;
		}
		for (Entity entity : lightEntities) {
			LightComponent* light = context->component->GetComponent<LightComponent>(entity);
			if (!light) {
				continue;
			}
			TransformComponent* transform = context->component->GetComponent<TransformComponent>(entity);
			if (transform) {
				newContext.cameraPosition = DirectX::XMFLOAT4(transform->position.x, transform->position.y, transform->position.z, 0.0f);

				Vector3 front = transform->front();
				Vector3 up = transform->up();

				newContext.viewMatrix = DirectX::XMMatrixLookAtLH(
					transform->position.ToXMVECTOR(),
					(transform->position + front * 100.0f).ToXMVECTOR(),
					(up).ToXMVECTOR()
				);
				newContext.projectionMatrix = DirectX::XMMatrixOrthographicLH(
					100.0f,
					100.0f,
					0.01f,
					100.0f
				);
				break;
				break;
			}
		}
	}


	bool* pRenderLayer = newContext.renderLayerVisibility;

	// コンテキストの取得
	GraphicsContext* graphicsContext = m_context->renderer->GetGraphicsContext();

	CAMERA camera{};
	camera.CameraPosition = newContext.cameraPosition;
	graphicsContext->SetCamera(camera);

	graphicsContext->SetViewMatrix(newContext.viewMatrix);
	graphicsContext->SetProjectionMatrix(newContext.projectionMatrix);

	D3D11_VIEWPORT vp = {};
	vp.Width = newContext.screenSize.x;
	vp.Height = newContext.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	deviceContext->RSSetViewports(1, &vp);

	m_context->imgui->SetViewProjectionMatrix(newContext.viewMatrix, newContext.projectionMatrix);

	for (int i = 0; i < (int)RenderLayer::MaxRenderLayer; i++) {

		if(newContext.renderLayerVisibility[i] == false){
			continue;
		}

		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();

			// コンポーネントを持つエンティティの検索
			std::vector<Entity> entities = context->component->FindEntitiesWithComponent<TransformComponent>();

			if (entities.empty()) {

				continue;

			} else {

				for (Entity entity : entities) {

					RenderLayer layer = scene->GetRenderLayerFromEntity(entity);

					if ((int)layer != i) {
						continue;
					}

					for(auto renderable : renderables){
						renderable->Execute(newContext, context ,entity);
					}
				}
			}
		}
	}
}
