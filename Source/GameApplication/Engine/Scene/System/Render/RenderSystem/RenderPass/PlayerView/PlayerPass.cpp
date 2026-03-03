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

	playerRenderTarget = new RenderTarget(
		context->PlayerScreenSize,
		context->graphics,
		RenderTargetType::RENDERTARGET_TYPE_COLOR
	);
}

void PlayerPass::Finalize() {

	postEffectPass->Finalize();
	delete postEffectPass;
	postEffectPass = nullptr;

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

	overlayUIPass->Finalize();
	delete overlayUIPass;
	overlayUIPass = nullptr;

	delete playerRenderTarget;
	playerRenderTarget = nullptr;
}

void PlayerPass::Execute(const RenderPassContext& ctx) {

	GraphicsContext*     graphicsContext = m_context->renderer->GetGraphicsContext();

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

	result = postEffectPass->resultSrv;

	// エディタ用レンダーターゲットをリサイズ (ウィンドウ描画に使用)
	float clearCol[4] = {0.0f, 1.0f, 0.0f, 1.0f};
	playerRenderTarget->Resize(ctx.screenSize, m_context->graphics);
	playerRenderTarget->Clear(m_context->graphics->GetDeviceContext(), clearCol);
}
