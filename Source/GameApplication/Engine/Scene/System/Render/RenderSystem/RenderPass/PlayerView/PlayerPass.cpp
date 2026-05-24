// =======================================================================
// 
// PlayerPass.cpp
// 
// =======================================================================
#include "PlayerPass.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderPhase.h"

#include "../ShadowMap/ShadowMapPass.h"
#include "../GBuffer/GBufferPass.h"
#include "../LightingPass/LightingPass.h"
#include "../Forward/ForwardPass.h"
#include "../PostEffect/PostEffectPass.h"
#include "../OverlayUI/OverlayUIPass.h"

#include "scene.h"
#include "System/Render/RenderSystem/Renderable/IRenderable.h"
#include "System/Render/RenderSystem/RenderTarget/renderTarget.h"
#include "Component/transformComponent.h"
#include "Registry/componentRegistry.h"

#include "Shader/commonDefine.h"

#include "Graphics/graphicsContext.h"
#include "Registry/systemRegistry.h"

#include "Component/CameraComponent.h"
#include "Component/LightComponent.h"

#include <Component/RenderLayerComponent.h>


void PlayerPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

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

	pPlayerRenderTarget = new RenderTarget(
		context->PlayerScreenSize,
		context->pGraphics,
		RenderTargetType::RENDERTARGET_TYPE_COLOR
	);
}

void PlayerPass::Finalize() {

	postEffectPass->Finalize();
	delete m_PostEffectPass;
	pPostEffectPass = nullptr;

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

	overlayUIPass->Finalize();
	delete m_OverlayUipass;
	pOverlayUIPass = nullptr;

	delete m_PlayerRenderTarget;
	pPlayerRenderTarget = nullptr;
}

void PlayerPass::Execute(const RenderPassContext& ctx) {

	if (ctx.cameraData.pCameraComponent == nullptr) {
		pResult = nullptr;
		return;
	}

	// レンダーターゲットをリサイズ (ウィンドウ描画に使用)
	float m_ClearCol[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	playerRenderTarget->Resize(ctx.screenSize, m_pContext->pGraphics);
	playerRenderTarget->Clear(m_pContext->pGraphics->GetDeviceContext(), clearCol);

	GraphicsContext*     graphicsContext = m_pContext->pRenderer->GetGraphicsContext();

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

	// フォワードパス (透明・UI)
	forwardPass->SetInputs(lightingPass, gBufferPass, shadowMapPass);
	forwardPass->Execute(ctx);

	// ポストエフェクトパス
	ID3D11ShaderResourceView* initialSRV = lightingPass->pRenderTarget->srv.Get();
	ID3D11RenderTargetView** initialRTV = lightingPass->pRenderTarget->rtv.GetAddressOf();
	postEffectPass->SetInputs(initialSRV, initialRTV,gBufferPass);
	postEffectPass->Execute(ctx);

	overlayUIPass->SetInputs(postEffectPass->resultRtv,lightingPass->pRenderTarget);
	overlayUIPass->Execute(ctx);

	pResult = pPostEffectPass->pResultSrv;
}
