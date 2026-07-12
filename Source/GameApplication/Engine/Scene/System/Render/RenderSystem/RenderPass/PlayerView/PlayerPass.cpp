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
#include "Resources/resourceService.h"
#include "Resources/Data/vertexShaderData.h"
#include "Resources/Data/pixelShaderData.h"

#include <algorithm>

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

	if(context && context->resource){
		auto vertexShader = context->resource->Load<VertexShaderData>(
			"Asset\\Shader\\PostEffectVS.cso"
		);
		auto pixelShader = context->resource->Load<PixelShaderData>(
			"Asset\\Shader\\PostEffectPS.cso"
		);
		if(vertexShader && pixelShader){
			m_runtimeUiCopyShader = std::make_unique<PostEffectShader>();
			m_runtimeUiCopyShader->m_VS = vertexShader->m_VertexShader;
			m_runtimeUiCopyShader->m_PS = pixelShader->m_PixelShader;
			m_runtimeUiCopyShader->m_InputLayout = vertexShader->m_VertexLayout;
		}
	}

	ID3D11Device* device = context && context->graphics
		? context->graphics->GetDevice()
		: nullptr;
	if(device){
		D3D11_BLEND_DESC blendDesc{};
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;
		auto& target = blendDesc.RenderTarget[0];
		target.BlendEnable = TRUE;
		// Direct2Dの透明TextureはPremultiplied Alpha。
		target.SrcBlend = D3D11_BLEND_ONE;
		target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		target.BlendOp = D3D11_BLEND_OP_ADD;
		target.SrcBlendAlpha = D3D11_BLEND_ONE;
		target.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		device->CreateBlendState(
			&blendDesc,
			m_runtimeUiBlendState.ReleaseAndGetAddressOf()
		);

		D3D11_DEPTH_STENCIL_DESC depthDesc{};
		depthDesc.DepthEnable = FALSE;
		depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		depthDesc.StencilEnable = FALSE;
		device->CreateDepthStencilState(
			&depthDesc,
			m_runtimeUiDepthState.ReleaseAndGetAddressOf()
		);
	}
}

void PlayerPass::Finalize(){
	m_runtimeUiDepthState.Reset();
	m_runtimeUiBlendState.Reset();
	m_runtimeUiCopyShader.reset();
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

	graphics->SetPerCameraConstants(
		viewContext.CameraPosition,
		viewContext.viewMatrix,
		viewContext.projectionMatrix
	);
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

	// CustomScriptのRuntime UI CommandをPlayerPass最終Textureへ合成する。
	// PlayViewはこのresultSrvを表示するため、Editor埋め込みとStandaloneで同一画面になる。
	const UINT overlayWidth = static_cast<UINT>(
		(std::max)(1.0f, viewContext.screenSize.x)
	);
	const UINT overlayHeight = static_cast<UINT>(
		(std::max)(1.0f, viewContext.screenSize.y)
	);
	ID3D11ShaderResourceView* runtimeUiSrv =
		m_context->renderer->RenderRuntime2DOverlay(overlayWidth, overlayHeight);

	ID3D11RenderTargetView* finalRtv =
		postEffectPass->resultRtv ? *postEffectPass->resultRtv : nullptr;
	if(runtimeUiSrv && finalRtv && m_runtimeUiCopyShader &&
		m_runtimeUiBlendState && m_runtimeUiDepthState){
		Microsoft::WRL::ComPtr<ID3D11BlendState> previousBlendState;
		FLOAT previousBlendFactor[4]{};
		UINT previousSampleMask = 0xffffffff;
		deviceContext->OMGetBlendState(
			previousBlendState.GetAddressOf(),
			previousBlendFactor,
			&previousSampleMask
		);

		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> previousDepthState;
		UINT previousStencilRef = 0;
		deviceContext->OMGetDepthStencilState(
			previousDepthState.GetAddressOf(),
			&previousStencilRef
		);

		deviceContext->OMSetRenderTargets(1, &finalRtv, nullptr);
		D3D11_VIEWPORT viewport{};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(overlayWidth);
		viewport.Height = static_cast<float>(overlayHeight);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		deviceContext->RSSetViewports(1, &viewport);

		const FLOAT blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		deviceContext->OMSetBlendState(
			m_runtimeUiBlendState.Get(),
			blendFactor,
			0xffffffff
		);
		deviceContext->OMSetDepthStencilState(m_runtimeUiDepthState.Get(), 0);
		graphics->DrawQuad(m_runtimeUiCopyShader.get(), runtimeUiSrv);

		deviceContext->OMSetBlendState(
			previousBlendState.Get(),
			previousBlendFactor,
			previousSampleMask
		);
		deviceContext->OMSetDepthStencilState(
			previousDepthState.Get(),
			previousStencilRef
		);
	}

	result = postEffectPass->resultSrv;
}
