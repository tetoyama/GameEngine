#include "PlayerPass.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../ShadowMap/ShadowMapPass.h"
#include "../GBuffer/GBufferPass.h"
#include "../LightingPass/LightingPass.h"
#include "../Forward/ForwardPass.h"
#include "../PostEffect/PostEffectPass.h"
#include "../OverlayUI/OverlayUIPass.h"
#include "System/Render/RenderSystem/RenderTarget/renderTarget.h"
#include "Graphics/graphicsContext.h"
#include "Graphics/mainRenderer.h"

void PlayerPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context){
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
	playerRenderTarget = new RenderTarget(context->PlayerScreenSize, context->graphics, RenderTargetType::RENDERTARGET_TYPE_COLOR);
}

void PlayerPass::Finalize(){
	postEffectPass->Finalize(); delete postEffectPass; postEffectPass = nullptr;
	forwardPass->Finalize(); delete forwardPass; forwardPass = nullptr;
	lightingPass->Finalize(); delete lightingPass; lightingPass = nullptr;
	gBufferPass->Finalize(); delete gBufferPass; gBufferPass = nullptr;
	shadowMapPass->Finalize(); delete shadowMapPass; shadowMapPass = nullptr;
	overlayUIPass->Finalize(); delete overlayUIPass; overlayUIPass = nullptr;
	delete playerRenderTarget; playerRenderTarget = nullptr;
}

void PlayerPass::Execute(const RenderPassContext& context){
	if(!context.cameraData.cameraComponent){ result = nullptr; return; }
	RenderPassContext viewContext = context;
	viewContext.cullingViewKind = CullingViewKind::Player;
	viewContext.cullingViewInstanceID = 0;
	m_renderSystem->PrepareRenderPacketView(viewContext);

	float clearColor[4] = {0.0f, 1.0f, 0.0f, 1.0f};
	playerRenderTarget->Resize(viewContext.screenSize, m_context->graphics);
	playerRenderTarget->Clear(m_context->graphics->GetDeviceContext(), clearColor);
	GraphicsContext* graphics = m_context->renderer->GetGraphicsContext();
	GpuPassTimingProfiler& profiler =
		m_context->renderer->GetGpuPassTimingProfiler();
	ID3D11DeviceContext* deviceContext = graphics->GetDeviceContext();

	{
		ScopedGpuPassTiming timing(
			profiler,
			deviceContext,
			GpuPassTimingScope::PlayerGBuffer
		);
		gBufferPass->Execute(viewContext);
	}
	{
		ScopedGpuPassTiming timing(
			profiler,
			deviceContext,
			GpuPassTimingScope::PlayerShadow
		);
		shadowMapPass->Execute(viewContext);
	}

	graphics->SetCameraPosition(viewContext.CameraPosition);
	graphics->SetViewMatrix(viewContext.viewMatrix);
	graphics->SetProjectionMatrix(viewContext.projectionMatrix);
	lightingPass->SetTextureSlot(gBufferPass, shadowMapPass, graphics);
	{
		ScopedGpuPassTiming timing(
			profiler,
			deviceContext,
			GpuPassTimingScope::PlayerLighting
		);
		lightingPass->Execute(viewContext);
	}

	forwardPass->SetInputs(lightingPass, gBufferPass, shadowMapPass);
	{
		ScopedGpuPassTiming timing(
			profiler,
			deviceContext,
			GpuPassTimingScope::PlayerForward
		);
		forwardPass->Execute(viewContext);
	}

	ID3D11ShaderResourceView* initialSRV = lightingPass->pRenderTarget->srv.Get();
	ID3D11RenderTargetView** initialRTV = lightingPass->pRenderTarget->rtv.GetAddressOf();
	postEffectPass->SetInputs(initialSRV, initialRTV, gBufferPass);
	{
		ScopedGpuPassTiming timing(
			profiler,
			deviceContext,
			GpuPassTimingScope::PlayerPostEffect
		);
		postEffectPass->Execute(viewContext);
	}

	overlayUIPass->SetInputs(postEffectPass->resultRtv, lightingPass->pRenderTarget);
	{
		ScopedGpuPassTiming timing(
			profiler,
			deviceContext,
			GpuPassTimingScope::PlayerOverlay
		);
		overlayUIPass->Execute(viewContext);
	}
	result = postEffectPass->resultSrv;
}
