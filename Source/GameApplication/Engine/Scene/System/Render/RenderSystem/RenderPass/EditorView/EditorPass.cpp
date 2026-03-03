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

	m_renderSystem = renderSystem;
	m_context = context;

	shadowMapPass = new ShadowMapPass();
	shadowMapPass->Initialize(renderSystem, context);

	gBufferPass = new GBufferPass();
	gBufferPass->Initialize(renderSystem, context);

	lightingPass = new LightingPass();
	lightingPass->Initialize(renderSystem, context);

	forwardPass = new ForwardPass();
	forwardPass->Initialize(renderSystem, context);

	postEffectPass = new PostEffectPass();
	postEffectPass->Initialize(renderSystem, context);

	overlayUIPass = new OverlayUIPass();
	overlayUIPass->Initialize(renderSystem, context);

	physXDebugPass = new PhysXDebugPass();
	physXDebugPass->Initialize(renderSystem, context);
}

void EditorPass::Finalize() {

	postEffectPass->Finalize();
	delete postEffectPass;
	postEffectPass = nullptr;

	overlayUIPass->Finalize();
	delete overlayUIPass;
	overlayUIPass = nullptr;

	forwardPass->Finalize();
	delete forwardPass;
	forwardPass = nullptr;

	lightingPass->Finalize();
	delete lightingPass;
	lightingPass = nullptr;

	gBufferPass->Finalize();
	delete gBufferPass;
	gBufferPass = nullptr;

	shadowMapPass->Finalize();
	delete shadowMapPass;
	shadowMapPass = nullptr;

	physXDebugPass->Finalize();
	delete physXDebugPass;
	physXDebugPass = nullptr;
}

void EditorPass::Execute(const RenderPassContext& ctx) {

	GraphicsContext* graphicsContext = m_context->renderer->GetGraphicsContext();

	// ImGuizmo 用のビュー・投影行列を設定
	m_context->imgui->SetViewProjectionMatrix(ctx.viewMatrix, ctx.projectionMatrix);

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

	result = postEffectPass->resultSrv;
}
