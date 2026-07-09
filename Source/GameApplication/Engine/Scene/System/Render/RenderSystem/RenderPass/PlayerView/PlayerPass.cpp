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

PlayerPass::PlayerPass() = default;
PlayerPass::~PlayerPass() = default;

void PlayerPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context){
	m_renderSystem = renderSystem;
	m_context = context;
	shadowMapPass = std::make_unique<ShadowMapPass>();
	shadowMapPass->Initialize(renderSystem, context);
	gBufferPass = std::make_unique<GBufferPass>();
	gBufferPass->Initialize(renderSystem, context);
	lightingPass = std::make_unique<LightingPass>();
	lightingPass->Initialize(renderSystem, context);
	forwardPass = std::make_unique<ForwardPass>();
	forwardPass->Initialize(renderSystem, context);
	postEffectPass = std::make_unique<PostEffectPass>();
	postEffectPass->Initialize(renderSystem, context);
	overlayUIPass = std::make_unique<OverlayUIPass>();
	overlayUIPass->Initialize(renderSystem, context);
	playerRenderTarget = std::make_unique<RenderTarget>(
		context->PlayerScreenSize,
		context->graphics,
		RenderTargetType::RENDERTARGET_TYPE_COLOR
	);
}

void PlayerPass::Finalize(){
	if(postEffectPass){ postEffectPass->Finalize(); postEffectPass.reset(); }
	if(forwardPass){ forwardPass->Finalize(); forwardPass.reset(); }
	if(lightingPass){ lightingPass->Finalize(); lightingPass.reset(); }
	if(gBufferPass){ gBufferPass->Finalize(); gBufferPass.reset(); }
	if(shadowMapPass){ shadowMapPass->Finalize(); shadowMapPass.reset(); }
	if(overlayUIPass){ overlayUIPass->Finalize(); overlayUIPass.reset(); }
	playerRenderTarget.reset();
	result = nullptr;
}

void PlayerPass::Execute(const RenderPassContext& context){
	if(!context.cameraData.cameraComponent){ result = nullptr; return; }
	if(!playerRenderTarget || !gBufferPass || !shadowMapPass || !lightingPass ||
		!forwardPass || !postEffectPass || !overlayUIPass){
		result = nullptr;
		return;
	}

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
	lightingPass->SetTextureSlot(gBufferPass.get(), shadowMapPass.get(), graphics);
	{
		ScopedGpuPassTiming timing(
			profiler,
			deviceContext,
			GpuPassTimingScope::PlayerLighting
		);
		lightingPass->Execute(viewContext);
	}

	forwardPass->SetInputs(lightingPass.get(), gBufferPass.get(), shadowMapPass.get());
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
	postEffectPass->SetInputs(initialSRV, initialRTV, gBufferPass.get());
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
