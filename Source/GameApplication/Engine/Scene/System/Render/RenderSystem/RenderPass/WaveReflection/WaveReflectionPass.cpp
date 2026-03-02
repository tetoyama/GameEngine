#include "WaveReflectionPass.h"

#include <DirectXMath.h>

#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderLayer.h"
#include "../../renderPhase.h"

#include "../ShadowMap/ShadowMapPass.h"

#include "scene.h"
#include "System/Render/RenderSystem/Renderable/IRenderable.h"
#include "System/Render/RenderSystem/RenderTarget/renderTarget.h"
#include "System/Render/RenderSystem/Renderable/Model/RenderableModel.h"
#include "System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.h"
#include "System/Render/RenderSystem/Renderable/Wave/RenderableWave.h"

#include "Component/transformComponent.h"
#include "Component/waveComponent.h"
#include "Component/materialComponent.h"
#include "Registry/componentRegistry.h"

#include "Graphics/graphicsContext.h"
#include "Resources/Data/shaderData.h"

#include "Shader/commonDefine.h"

using namespace DirectX;

//----------------------------------------------------------------------
void WaveReflectionPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

	m_renderSystem = renderSystem;
	m_context      = context;

	m_vertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");
	m_pixelShader  = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\ForwardRenderingPS.cso");

	Vector2 size = Vector2(
		(float)context->graphics->m_width,
		(float)context->graphics->m_height
	);
	reflectionRT = new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_COLOR);

	// Waveを除いた反射対象レンダラブル
	m_renderables.clear();
	m_renderables.push_back(renderSystem->GetRenderable<RenderableModel>());
	m_renderables.push_back(renderSystem->GetRenderable<RenderableTerrain>());
}

//----------------------------------------------------------------------
void WaveReflectionPass::Finalize() {

	m_vertexShader.reset();
	m_pixelShader.reset();

	delete reflectionRT;
	reflectionRT = nullptr;
}

//----------------------------------------------------------------------
void WaveReflectionPass::SetInputs(ShadowMapPass* shadowMapPass) {
	m_shadowMapPass = shadowMapPass;
}

//----------------------------------------------------------------------
float WaveReflectionPass::GetWaterPlaneY(const RenderPassContext& ctx) const {

	float sum   = 0.0f;
	int   count = 0;

	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto* sctx = scene->GetSceneContext();
		auto entities = sctx->component->FindEntitiesWithComponent<WaveComponent>();

		for (auto entity : entities) {
			auto* wave = sctx->component->GetComponent<WaveComponent>(entity);
			if (!wave || !wave->ReflectionEnabled) continue;

			auto* tf = sctx->component->GetComponent<TransformComponent>(entity);
			if (!tf) continue;

			Vector3 worldPos = tf->GetWorldPosition(sctx->component);
			sum += worldPos.y;
			count++;
		}
	}

	return count > 0 ? sum / static_cast<float>(count) : 0.0f;
}

//----------------------------------------------------------------------
void WaveReflectionPass::Execute(const RenderPassContext& ctx) {

	// 反射有効な Wave エンティティが存在するか確認
	bool hasReflection = false;
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto* sctx = scene->GetSceneContext();
		auto entities = sctx->component->FindEntitiesWithComponent<WaveComponent>();
		for (auto entity : entities) {
			auto* wave = sctx->component->GetComponent<WaveComponent>(entity);
			if (wave && wave->ReflectionEnabled) {
				hasReflection = true;
				break;
			}
		}
		if (hasReflection) break;
	}

	// 反射 SRV を RenderableWave に渡す（反射なし時は nullptr）
	RenderableWave* rw = m_renderSystem->GetRenderable<RenderableWave>();
	if (rw) {
		rw->reflectionSRV = hasReflection ? reflectionRT->srv.Get() : nullptr;
	}

	if (!hasReflection) return;

	float waterY = GetWaterPlaneY(ctx);

	// カメラが水面より下の場合は反射をスキップ
	if (ctx.CameraPosition.y < waterY) {
		if (rw) rw->reflectionSRV = nullptr;
		return;
	}

	// ---- 反射ビュー行列の計算 ----
	// 水平面 Y = waterY での反射行列を生成し、既存ビュー行列に合成する
	XMVECTOR reflPlane   = XMVectorSet(0.0f, 1.0f, 0.0f, -waterY);
	XMMATRIX reflMatrix  = XMMatrixReflect(reflPlane);
	XMMATRIX reflView    = XMMatrixMultiply(reflMatrix, ctx.viewMatrix);

	// 反射カメラ位置
	float reflCamY = 2.0f * waterY - ctx.CameraPosition.y;
	XMFLOAT4 reflCamPos = XMFLOAT4(
		ctx.CameraPosition.x,
		reflCamY,
		ctx.CameraPosition.z,
		1.0f
	);

	GraphicsContext*     gc = m_context->graphics;
	ID3D11DeviceContext* dc = gc->GetDeviceContext();

	// ---- レンダーターゲット設定 ----
	reflectionRT->Resize(ctx.screenSize, gc);
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	reflectionRT->Clear(dc, clearColor);

	dc->OMSetRenderTargets(
		1,
		reflectionRT->rtv.GetAddressOf(),
		reflectionRT->dsv.Get()
	);

	// ---- シェーダー設定 ----
	dc->VSSetShader(m_vertexShader->m_VertexShader.Get(), nullptr, 0);
	dc->IASetInputLayout(m_vertexShader->m_VertexLayout.Get());
	if (m_pixelShader) {
		dc->PSSetShader(m_pixelShader->m_PixelShader.Get(), nullptr, 0);
	}

	// シャドウマップのバインド（利用可能な場合）
	if (m_shadowMapPass) {
		dc->PSSetShaderResources(
			TextureSlot_ShadowMap, 1,
			m_shadowMapPass->shadowRenderTarget->srv.GetAddressOf()
		);
		dc->PSSetSamplers(1, 1, &m_shadowMapPass->shadowSampler);
	}

	// ---- 反射カメラのビュー行列・カメラ位置をセット ----
	gc->SetCameraPosition(reflCamPos);
	gc->SetViewMatrix(reflView);
	gc->SetProjectionMatrix(ctx.projectionMatrix);

	// 反射時はポリゴンの表裏が反転するためフロントカリングに変更
	gc->SetCullMode(CullMode::Front);
	gc->SetBlendMode(BlendMode::None);
	gc->SetDepthMode(DepthMode::Write);

	// ---- ビューポート設定 ----
	D3D11_VIEWPORT vp{};
	vp.Width    = ctx.screenSize.x;
	vp.Height   = ctx.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	dc->RSSetViewports(1, &vp);

	// ---- 不透明レイヤーのみ反射描画 ----
	RenderPassContext reflCtx    = ctx;
	reflCtx.viewMatrix           = reflView;
	reflCtx.CameraPosition       = reflCamPos;

	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto* sctx    = scene->GetSceneContext();
		auto entities = sctx->component->FindEntitiesWithComponent<TransformComponent>();

		for (auto entity : entities) {
			RenderLayer layer = scene->GetRenderLayerFromEntity(entity);

			// 透明・UI・デバッグレイヤーは反射しない
			if (layer == RenderLayer::SortTransparent3D ||
				layer == RenderLayer::Transparent3D     ||
				layer == RenderLayer::OverlayUI         ||
				layer == RenderLayer::Debug) {
				continue;
			}

			if (!reflCtx.renderLayerVisibility[(int)layer]) continue;

			for (auto* r : m_renderables) {
				int materialID = 0;
				auto* mat = sctx->component->GetComponent<MaterialComponent>(entity);
				if (mat) materialID = mat->ShaderID;

				ObjectInfo info;
				info.SceneID  = (unsigned int)sctx;
				info.ObjectID = entity;
				info.ShaderID = materialID;
				gc->SetObjectInfo(info);

				r->Execute(reflCtx, sctx, entity);
			}
		}
	}

	// ---- 状態を元に戻す ----
	gc->SetCullMode(CullMode::Back);
	gc->SetCameraPosition(ctx.CameraPosition);
	gc->SetViewMatrix(ctx.viewMatrix);
	gc->SetProjectionMatrix(ctx.projectionMatrix);

	// レンダーターゲットを解除
	ID3D11RenderTargetView* nullRTV[1] = { nullptr };
	dc->OMSetRenderTargets(1, nullRTV, nullptr);
}
