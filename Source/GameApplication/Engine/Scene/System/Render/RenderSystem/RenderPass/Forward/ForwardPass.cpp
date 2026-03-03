#include "ForwardPass.h"
#include "Shader/commonDefine.h"

#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderPhase.h"
#include "../../renderLayer.h"

#include "../GBuffer/GBufferPass.h"
#include "../LightingPass/LightingPass.h"
#include "../ShadowMap/ShadowMapPass.h"

#include "scene.h"
#include "System/Render/RenderSystem/Renderable/IRenderable.h"
#include "System/Render/RenderSystem/RenderTarget/renderTarget.h"
#include "Component/transformComponent.h"
#include "Registry/componentRegistry.h"

#include "Graphics/graphicsContext.h"
#include "Registry/systemRegistry.h"

#include "Component/CameraComponent.h"
#include "Component/LightComponent.h"

#include "Resources/Data/textureData.h"

#include "System/Render/RenderSystem/Renderable/Model/RenderableModel.h"
#include "System/Render/RenderSystem/Renderable/BillBoard/RenderableBillBoard.h"
#include "System/Render/RenderSystem/Renderable/Mesh/RenderableMesh.h"
#include "System/Render/RenderSystem/Renderable/Particle/RenderableParticle.h"
#include "System/Render/RenderSystem/Renderable/Sprite/RenderableSprite.h"
#include "System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.h"
#include "System/Render/RenderSystem/Renderable/Wave/RenderableWave.h"
#include "System/Render/RenderSystem/Renderable/Effect/RenderableEffect.h"

#include <algorithm>
#include <Component/outlineComponent.h>
#include <Component/materialComponent.h>
#include <Component/RenderLayerComponent.h>

void ForwardPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

	m_renderSystem = renderSystem;
	m_context = context;

	m_VertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");

	renderables.clear();
	renderables.push_back(renderSystem->GetRenderable<RenderableModel>());
	renderables.push_back(renderSystem->GetRenderable<RenderableBillBoard>());
	renderables.push_back(renderSystem->GetRenderable<RenderableMesh>());
	renderables.push_back(renderSystem->GetRenderable<RenderableParticle>());
	renderables.push_back(renderSystem->GetRenderable<RenderableSprite>());
	renderables.push_back(renderSystem->GetRenderable<RenderableTerrain>());
	renderables.push_back(renderSystem->GetRenderable<RenderableWave>());
	renderables.push_back(renderSystem->GetRenderable<RenderableEffect>());
}

void ForwardPass::Finalize() {
	m_VertexShader.reset();
}

void ForwardPass::SetInputs(LightingPass* lightingPass, GBufferPass* gBufferPass, ShadowMapPass* shadowMapPass) {
	m_lightingPass  = lightingPass;
	m_gBufferPass   = gBufferPass;
	m_shadowMapPass = shadowMapPass;
}

void ForwardPass::Execute(const RenderPassContext& ctx) {

	GraphicsContext* graphics = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphics->GetDeviceContext();

	// レンダーターゲット設定 (ライティング結果テクスチャに書き込む)
	deviceContext->OMSetRenderTargets(
		1,
		m_lightingPass->pRenderTarget->rtv.GetAddressOf(),
		m_gBufferPass->pDepthTarget->dsv.Get()
	);

	// シャドウマップバインド
	deviceContext->PSSetShaderResources(TextureSlot_ShadowMap, 1, m_shadowMapPass->shadowRenderTarget->srv.GetAddressOf());
	deviceContext->PSSetSamplers(1, 1, &m_shadowMapPass->shadowSampler);

	// 環境マップバインド (メタリックオブジェクト用)
	if(m_lightingPass->m_EnvironmentMap && m_lightingPass->m_EnvironmentMap->pTexture){
		deviceContext->PSSetShaderResources(TextureSlot_EnvironmentMap, 1, m_lightingPass->m_EnvironmentMap->pTexture.GetAddressOf());
	}
	if(m_lightingPass->m_EnvMapSampler){
		deviceContext->PSSetSamplers(3, 1, &m_lightingPass->m_EnvMapSampler);
	}

	// シェーダーセット
	deviceContext->VSSetShader(m_VertexShader->m_VertexShader.Get(), nullptr, 0);
	deviceContext->IASetInputLayout(m_VertexShader->m_VertexLayout.Get());
	PixelShaderData* ps = m_renderSystem->GetForwardPS();
	deviceContext->PSSetShader(ps ? ps->m_PixelShader.Get() : nullptr, nullptr, 0);

	// ビューポート設定
	D3D11_VIEWPORT vp = {};
	vp.Width = ctx.screenSize.x;
	vp.Height = ctx.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	deviceContext->RSSetViewports(1, &vp);

	graphics->SetBlendMode(BlendMode::Alpha);

	// 透明・UIレイヤーのみ描画
	for (int i = 0; i < (int)RenderLayer::MaxRenderLayer; i++) {

		if (!ctx.renderLayerVisibility[i]) {
			continue;
		}

		if (i != (int)RenderLayer::SortTransparent3D &&
			i != (int)RenderLayer::Transparent3D) {
			continue;
		}

		std::vector<TransparentDrawItem> transparentList;

		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {

			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<TransformComponent>();
			if (entities.empty()) {
				continue;
			}

			for (Entity entity : entities) {

				RenderLayer layer = scene->GetRenderLayerFromEntity(entity);
				if ((int)layer != i) {
					continue;
				}

				if (layer == RenderLayer::SortTransparent3D) {

					auto transform = context->component->GetComponent<TransformComponent>(entity);
					if (!transform) {
						continue;
					}

					Vector3 worldPos = transform->GetWorldPosition(context->component);
					Vector3 diff = worldPos - Vector3(ctx.CameraPosition.x, ctx.CameraPosition.y, ctx.CameraPosition.z);

					TransparentDrawItem item;
					item.ref = EntityRef(entity, context);
					item.distanceSq = diff.dot(diff);
					transparentList.push_back(item);


				}
			}
		}

		// Z ソートして半透明描画
		if (!transparentList.empty()) {

			m_context->graphics->SetDepthMode(DepthMode::ReadOnly);

			std::sort(
				transparentList.begin(), transparentList.end(),
				[](const TransparentDrawItem& a, const TransparentDrawItem& b) {
				return a.distanceSq > b.distanceSq; // 遠い→近い
			}
			);

			for (auto& item : transparentList) {

				if (!item.ref.IsValid()) continue;
				Entity       entity = item.ref.GetEntityID();
				SceneContext* itemCtx = item.ref.GetScene();

				for (auto renderable : renderables) {

					int materialID = 0;
					auto material = itemCtx->component->GetComponent<MaterialComponent>(entity);
					if (material) {
						materialID = material->ShaderID;
					}

					ObjectInfo info;
					info.SceneID = (unsigned int)itemCtx;
					info.ObjectID = entity;
					info.ShaderID = materialID;
					m_context->graphics->SetObjectInfo(info);

					renderable->Execute(ctx, itemCtx, entity);
				}
			}

			m_context->graphics->SetDepthMode(DepthMode::Write);
		}
	}

	// RTV を解除
	ID3D11RenderTargetView* nullRTV[1] = { nullptr };
	deviceContext->OMSetRenderTargets(1, nullRTV, nullptr);
}
