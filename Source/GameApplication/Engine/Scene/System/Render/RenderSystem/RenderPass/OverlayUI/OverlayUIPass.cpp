// =======================================================================
// 
// OverlayUIPass.cpp
// 
// =======================================================================
#include "OverlayUIPass.h"
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

void OverlayUIPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

	m_renderSystem = renderSystem;
	m_context = context;
	m_VertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");

	renderables.clear();
	renderables.push_back(renderSystem->GetRenderable<RenderableSprite>());
}

void OverlayUIPass::Finalize() {
	m_VertexShader.reset();

}

void OverlayUIPass::Execute(const RenderPassContext& ctx) {

	GraphicsContext* graphics = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphics->GetDeviceContext();

	// レンダーターゲット設定 (ライティング結果テクスチャに書き込む)
	deviceContext->OMSetRenderTargets(
		1,
		m_rtv,
		m_dsv->dsv.Get()
	);

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

		if (i != (int)RenderLayer::OverlayUI) {
			continue;
		}

		std::vector<SpriteDrawItem>      spriteList;

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

				if (layer == RenderLayer::OverlayUI) {

					auto transform = context->component->GetComponent<TransformComponent>(entity);
					if (!transform) {
						continue;
					}

					SpriteDrawItem item;
					item.ref = EntityRef(entity, context);
					item.orderInLayer = 0;
					OrderInLayerComponent* layerComp = context->component->GetComponent<OrderInLayerComponent>(entity);
					if (layerComp) {
						item.orderInLayer = layerComp->order;
					}
					spriteList.push_back(item);

				} else {

					for (auto renderable : renderables) {

						int materialID = 0;
						auto material = context->component->GetComponent<MaterialComponent>(entity);
						if (material) {
							materialID = material->ShaderID;
						}

						ObjectInfo info;
						info.SceneID = m_context->sceneManager->GetIDFromContext(context);
						info.ObjectID = entity;
						info.ShaderID = materialID;
						m_context->graphics->SetObjectInfo(info);

						renderable->Execute(ctx, context, entity);
					}
				}
			}
		}

		// オーダーソートしてスプライト描画
		if (!spriteList.empty()) {

			std::sort(
				spriteList.begin(), spriteList.end(),
				[](const SpriteDrawItem& a, const SpriteDrawItem& b) {
				return a.orderInLayer > b.orderInLayer;
			}
			);

			for (auto& item : spriteList) {

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
					info.SceneID = m_context->sceneManager->GetIDFromContext(itemCtx);
					info.ObjectID = entity;
					info.ShaderID = materialID;
					m_context->graphics->SetObjectInfo(info);

					renderable->Execute(ctx, itemCtx, entity);
				}
			}
		}
	}

	// RTV を解除
	ID3D11RenderTargetView* nullRTV[1] = { nullptr };
	deviceContext->OMSetRenderTargets(1, nullRTV, nullptr);
}

void OverlayUIPass::SetInputs(ID3D11RenderTargetView** setRTV, RenderTarget* setDSV) {
	m_rtv = setRTV;
	m_dsv = setDSV;
}
