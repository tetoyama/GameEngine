// =======================================================================
// 
// EditorPass.cpp
// 
// =======================================================================
#include "EditorPass.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderPhase.h"
#include "../../RenderTarget/renderTarget.h"

#include "../ShadowMap/ShadowMapPass.h"
#include "../GBuffer/GBufferPass.h"
#include "../LightingPass/LightingPass.h"
#include "../Forward/ForwardPass.h"
#include "../PostEffect/PostEffectPass.h"
#include "../OverlayUI/OverlayUIPass.h"
#include "../PhysXDebug/PhysXDebugPass.h"

#include "Graphics/graphicsContext.h"
#include "DebugTools/ImGuiSystem.h"
#include "Service/Graphics/mainRenderer.h"


void EditorPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

	m_pRenderSystem = renderSystem;
	m_pContext = pContext;

	pShadowMapPass = new ShadowMapPass();
	shadowMapPass->Initialize(renderSystem, context);

	pGBufferPass = new GBufferPass();
	gBufferPass->Initialize(renderSystem, context);

	pLightingPass = new LightingPass();
	lightingPass->Initialize(renderSystem, context);

	pForwardPass = new ForwardPass();
	forwardPass->Initialize(renderSystem, context);

	pPostEffectPass = new PostEffectPass();
	postEffectPass->Initialize(renderSystem, context);

	pOverlayUIPass = new OverlayUIPass();
	overlayUIPass->Initialize(renderSystem, context);

	pPhysXDebugPass = new PhysXDebugPass();
	physXDebugPass->Initialize(renderSystem, context);
}

void EditorPass::Finalize() {

	postEffectPass->Finalize();
	delete m_PostEffectPass;
	pPostEffectPass = nullptr;

	overlayUIPass->Finalize();
	delete m_OverlayUipass;
	pOverlayUIPass = nullptr;

	forwardPass->Finalize();
	delete m_ForwardPass;
	pForwardPass = nullptr;

	lightingPass->Finalize();
	delete m_LightingPass;
	pLightingPass = nullptr;

	gBufferPass->Finalize();
	delete m_GBufferPass;
	pGBufferPass = nullptr;

	shadowMapPass->Finalize();
	delete m_ShadowMapPass;
	pShadowMapPass = nullptr;

	physXDebugPass->Finalize();
	delete m_PhysXdebugPass;
	pPhysXDebugPass = nullptr;
}

void EditorPass::Execute(const RenderPassContext& ctx) {

	GraphicsContext* graphicsContext = m_pContext->pRenderer->GetGraphicsContext();

	// ImGuizmo 用のビュー・投影行列を設定
	m_pContext->pImGui->SetViewProjectionMatrix(ctx.viewMatrix, ctx.projectionMatrix);

	// GBuffer パス
	gBufferPass->Execute(ctx);

	// シャドウマップパス
	shadowMapPass->Execute(ctx);

	// カメラ行列をセット (ライティング・フォワード両パスで共用)
	graphicsContext->SetCameraPosition(ctx.CameraPosition);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);

	// ライティングパス (Deferred)
	lightingPass->SetTextureSlot(gBufferPass, shadowMapPass, graphicsContext);
	lightingPass->Execute(ctx);

	// フォワードパス (透明・UIレイヤー)
	forwardPass->SetInputs(lightingPass, gBufferPass, shadowMapPass);
	forwardPass->Execute(ctx);

	// ポストエフェクトパス
	ID3D11ShaderResourceView* initialSRV = lightingPass->pRenderTarget->srv.Get();
	ID3D11RenderTargetView** initialRTV  = lightingPass->pRenderTarget->rtv.GetAddressOf();
	postEffectPass->SetInputs(initialSRV, initialRTV, gBufferPass);
	postEffectPass->Execute(ctx);

	// オーバーレイUIパス
	overlayUIPass->SetInputs(postEffectPass->resultRtv, lightingPass->pRenderTarget);
	overlayUIPass->Execute(ctx);

	// PhysX デバッグ描画 (ポストエフェクト後のバッファに直接描画)
	graphicsContext->GetDeviceContext()->OMSetRenderTargets(1, postEffectPass->resultRtv, gBufferPass->pDepthTarget->dsv.Get());
	physXDebugPass->Execute(ctx);
	ID3D11RenderTargetView* nullRTV[1] = { nullptr };
	graphicsContext->GetDeviceContext()->OMSetRenderTargets(1, nullRTV, nullptr);

	pResult = pPostEffectPass->pResultSrv;
}
