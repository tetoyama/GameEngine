// =======================================================================
// 
// ForwardPass.cpp
// 
// =======================================================================
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

	m_pRenderSystem = renderSystem;
	m_pContext = pContext;

	m_VertexShader = m_pContext->pResource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");

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
	m_pLightingPass  = pLightingPass;
	m_pGBufferPass   = pGBufferPass;
	m_pShadowMapPass = pShadowMapPass;
}

void ForwardPass::Execute(const RenderPassContext& ctx) {

	GraphicsContext* pGraphics = m_pContext->pGraphics;
	ID3D11DeviceContext* deviceContext = graphics->GetDeviceContext();

	// レンダーターゲット設定 (ライティング結果テクスチャに書き込む)
	deviceContext->OMSetRenderTargets(
		1,
		m_pLightingPass->pRenderTarget->rtv.GetAddressOf(),
		m_pGBufferPass->pDepthTarget->dsv.Get()
	);

	// シャドウマップバインド
	deviceContext->PSSetShaderResources(TextureSlot_ShadowMap, 1, m_pShadowMapPass->pShadowRenderTarget->srv.GetAddressOf());
	deviceContext->PSSetSamplers(1, 1, &m_pShadowMapPass->pShadowSampler);

	// 環境マップバインド (メタリックオブジェクト用)
	if(m_pLightingPass->m_EnvironmentMap && m_pLightingPass->m_EnvironmentMap->pTexture){
		deviceContext->PSSetShaderResources(TextureSlot_EnvironmentMap, 1, m_pLightingPass->m_EnvironmentMap->pTexture.GetAddressOf());
	}
	if(m_pLightingPass->m_EnvMapSampler){
		deviceContext->PSSetSamplers(3, 1, &m_pLightingPass->m_EnvMapSampler);
	}

	// シェーダーセット
	deviceContext->VSSetShader(m_VertexShader->m_VertexShader.Get(), nullptr, 0);
	deviceContext->IASetInputLayout(m_VertexShader->m_VertexLayout.Get());
	PixelShaderData* ps = m_pRenderSystem->GetForwardPS();
	deviceContext->PSSetShader(ps ? ps->m_PixelShader.Get() : nullptr, nullptr, 0);

	// ビューポート設定
	D3D11_VIEWPORT m_Vp= {};
	vp.Width = ctx.screenSize.x;
	vp.Height = ctx.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	deviceContext->RSSetViewports(1, &vp);

	graphics->SetBlendMode(BlendMode::Alpha);
	m_pContext->pGraphics->SetDepthMode(DepthMode::ReadOnly);

	// 透明・UIレイヤーのみ描画
	for (int i = 0; i < (int)RenderLayer::MaxRenderLayer; i++) {

		if (!ctx.renderLayerVisibility[i]) {
			continue;
		}

		if (i != (int)RenderLayer::SortTransparent3D &&
			i != (int)RenderLayer::Transparent3D) {
			continue;
		}

		std::vector<TransparentDrawItem> m_TransparentList;

		for (auto& [name, scene] : m_pContext->pSceneManager->GetActiveScenes()) {

			auto m_Context= scene->GetSceneContext();
			auto m_Entities= pContext->pComponent->FindEntitiesWithComponent<TransformComponent>();
			if (entities.empty()) {
				continue;
			}

			for (Entity entity : entities) {

				RenderLayer m_Layer= scene->GetRenderLayerFromEntity(pEntity);
				if ((int)layer != i) {
					continue;
				}

				if (layer == RenderLayer::SortTransparent3D) {

					auto m_Transform= pContext->pComponent->GetComponent<TransformComponent>(pEntity);
					if (!transform) {
						continue;
					}

					Vector3 m_WorldPos= transform->GetWorldPosition(pContext->pComponent);
					Vector3 m_Diff= worldPos - Vector3(ctx.CameraPosition.x, ctx.CameraPosition.y, ctx.CameraPosition.z);

					TransparentDrawItem m_Item;
					item.ref = EntityRef(pEntity, pContext);
					item.distanceSq = diff.dot(diff);
					transparentList.push_back(item);


				} else{
					for (auto renderable : renderables) {
						int m_MaterialId= 0;
						auto m_Material= pContext->pComponent->GetComponent<MaterialComponent>(pEntity);
						if (material) {
							materialID = material->ShaderID;
						}
						ObjectInfo m_Info;
						info.SceneID = m_pContext->pSceneManager->GetIDFromContext(pContext);
						info.ObjectID = pEntity;
						info.ShaderID = materialID;
						m_pContext->pGraphics->SetObjectInfo(info);
						renderable->Execute(ctx, context, entity);
					}
				}
			}
		}

		// Z ソートして半透明描画
		if (!transparentList.empty()) {


			std::sort(
				transparentList.begin(), transparentList.end(),
				[](const TransparentDrawItem& a, const TransparentDrawItem& b) {
				return a.distanceSq > b.distanceSq; // 遠い→近い
			}
			);

			for (auto& item : transparentList) {

				if (!item.ref.IsValid()) continue;
				Entity m_Entity= item.ref.GetEntityID();
				SceneContext* itemCtx = item.ref.GetScene();

				for (auto renderable : renderables) {

					int m_MaterialId= 0;
					auto m_Material= itemCtx->pComponent->GetComponent<MaterialComponent>(pEntity);
					if (material) {
						materialID = material->ShaderID;
					}

					ObjectInfo m_Info;
					info.SceneID = m_pContext->pSceneManager->GetIDFromContext(itemCtx);
					info.ObjectID = pEntity;
					info.ShaderID = materialID;
					m_pContext->pGraphics->SetObjectInfo(info);

					renderable->Execute(ctx, itemCtx, entity);
				}
			}

		}
	}

	m_pContext->pGraphics->SetDepthMode(DepthMode::Write);
	// RTV を解除
	ID3D11RenderTargetView* nullRTV[1] = { nullptr };
	deviceContext->OMSetRenderTargets(1, nullRTV, nullptr);
}
