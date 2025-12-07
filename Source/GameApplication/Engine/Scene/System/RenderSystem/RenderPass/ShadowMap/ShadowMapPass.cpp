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
#include <Component/cameraComponent.h>
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
void ShadowMapPass::Execute(const RenderPassContext& ctx){

	using namespace DirectX;

	ID3D11DeviceContext* deviceContext = m_context->graphics->GetDeviceContext();
	GraphicsContext* graphicsContext = m_context->renderer->GetGraphicsContext();

	ID3D11ShaderResourceView* nullSRV[1] = {nullptr};
	deviceContext->PSSetShaderResources(2, 1, nullSRV);
	deviceContext->PSSetShader(nullptr, NULL, 0);

	// ======== RenderTarget 切り替え ========
	if(shadowRenderTarget->type == RenderTargetType::RENDERTARGET_TYPE_DEPTH){
		deviceContext->ClearDepthStencilView(shadowRenderTarget->dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		deviceContext->OMSetRenderTargets(0, nullptr, shadowRenderTarget->dsv.Get());
	} else{
		float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		deviceContext->ClearRenderTargetView(shadowRenderTarget->rtv.Get(), color);
		deviceContext->ClearDepthStencilView(shadowRenderTarget->dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		deviceContext->OMSetRenderTargets(1, shadowRenderTarget->rtv.GetAddressOf(), shadowRenderTarget->dsv.Get());
	}

	RenderPassContext newContext = ctx;
	newContext.passPhase = RenderPhase::PHASE_SHADOW;
	newContext.screenSize = Vector2(SHADOWMAP_SIZE, SHADOWMAP_SIZE);

	// ======== メインカメラ取得 ========
	Vector3 mainCamPos = Vector3(ctx.cameraPosition.x, ctx.cameraPosition.y, ctx.cameraPosition.z);
	Vector3 mainCamFront = ctx.cameraData.transformComponent->front();

	// ======== Directional Light 取得 ========
	LIGHT lights[LIGHT_MAX_COUNT];

	// 全ライトを無効化で初期化
	for(int i = 0; i < LIGHT_MAX_COUNT; i++){
		lights[i].Enable = false;
	}
	int lightCount = 0;

	bool foundLight = false;
	for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
		auto sctx = scene->GetSceneContext();
		const auto& lightEntities = sctx->component->FindEntitiesWithComponent<LightComponent>();
		if(lightEntities.empty()) continue;

		for(Entity ent : lightEntities){
			LightComponent* light = sctx->component->GetComponent<LightComponent>(ent);
			if(!light) continue;

			// 最大数を超えたら安全に打ち切る
			if(lightCount >= LIGHT_MAX_COUNT){
				break;
			}

			TransformComponent* transform = sctx->component->GetComponent<TransformComponent>(ent);
			if(transform){
				if(light->light.LightType == LIGHT_TYPE_DIRECTIONAL){
					// ======== シャドウカメラ計算 ========
					float shadowDistance = 50.0f;
					float shadowSize = 80.0f;

					XMVECTOR camPos = mainCamPos.ToXMVECTOR();
					XMVECTOR camDir = mainCamFront.ToXMVECTOR();
					XMVECTOR ld = transform->front().ToXMVECTOR();

					XMVECTOR center = camPos;
					float dist = shadowSize * 2.0f;

					XMVECTOR eyev = center - ld * dist;
					XMVECTOR upv = XMVectorSet(0, 1, 0, 0);

					XMMATRIX lightView = XMMatrixLookAtLH(eyev, center, upv);
					XMMATRIX lightProj = XMMatrixOrthographicLH(shadowSize, shadowSize, 0.1f, dist * 3.0f);

					XMFLOAT3 lightCamPos;
					XMStoreFloat3(&lightCamPos, eyev);
					newContext.cameraPosition = XMFLOAT4(lightCamPos.x, lightCamPos.y, lightCamPos.z, 0);
					newContext.viewMatrix = lightView;
					newContext.projectionMatrix = lightProj;
					light->light.Position = newContext.cameraPosition;
					XMStoreFloat4x4(&light->light.LightView, DirectX::XMMatrixTranspose(newContext.viewMatrix));
					XMStoreFloat4x4(&light->light.LightProjection, DirectX::XMMatrixTranspose(newContext.projectionMatrix));

					foundLight = true;
				}
				light->light.Position = DirectX::XMFLOAT4(transform->position.x, transform->position.y, transform->position.z, 0.0f);
				light->light.Direction = DirectX::XMFLOAT4(transform->front().x, transform->front().y, transform->front().z, 0.0f);
				light->light.Enable = light->light.Enable;
			}
			// 現在のライトスロットにコピー
			lights[lightCount] = light->light;
			lightCount++;
		}
	}
	graphicsContext->SetLight(&lights[0]);

	if(!foundLight){
		return; // ライトが無い場合は中断
	}


	// ======== GraphicsContext に反映 ========
	CAMERA camSetter{};
	camSetter.CameraPosition = newContext.cameraPosition;
	graphicsContext->SetCamera(camSetter);
	graphicsContext->SetViewMatrix(newContext.viewMatrix);
	graphicsContext->SetProjectionMatrix(newContext.projectionMatrix);

	// ======== Viewport ========
	D3D11_VIEWPORT vp = {};
	vp.Width = newContext.screenSize.x;
	vp.Height = newContext.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	deviceContext->RSSetViewports(1, &vp);

	m_context->imgui->SetViewProjectionMatrix(newContext.viewMatrix, newContext.projectionMatrix);

	// ======== レンダリング実行 ========
	for(int i = 0; i < (int)RenderLayer::MaxRenderLayer; i++){

		if(!newContext.renderLayerVisibility[i]) continue;

		for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
			auto sctx = scene->GetSceneContext();

			std::vector<Entity> entities = sctx->component->FindEntitiesWithComponent<TransformComponent>();
			if(entities.empty()) continue;

			for(Entity ent : entities){

				RenderLayer layer = scene->GetRenderLayerFromEntity(ent);
				if((int)layer != i) continue;

				for(auto renderable : renderables){
					renderable->Execute(newContext, sctx, ent);
				}
			}
		}
	}

	ImGui::Image((ImTextureRef)shadowRenderTarget->srv.Get(), ImVec2(200, 200));
}

