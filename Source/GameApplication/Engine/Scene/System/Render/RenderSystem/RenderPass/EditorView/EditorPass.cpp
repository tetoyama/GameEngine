#include "EditorPass.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "../RenderPassContext.h"
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

void EditorPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context){
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

void EditorPass::Finalize(){
	postEffectPass->Finalize(); delete postEffectPass; postEffectPass = nullptr;
	overlayUIPass->Finalize(); delete overlayUIPass; overlayUIPass = nullptr;
	forwardPass->Finalize(); delete forwardPass; forwardPass = nullptr;
	lightingPass->Finalize(); delete lightingPass; lightingPass = nullptr;
	gBufferPass->Finalize(); delete gBufferPass; gBufferPass = nullptr;
	shadowMapPass->Finalize(); delete shadowMapPass; shadowMapPass = nullptr;
	physXDebugPass->Finalize(); delete physXDebugPass; physXDebugPass = nullptr;
}

void EditorPass::Execute(const RenderPassContext& context){
	RenderPassContext viewContext = context;
	viewContext.cullingViewKind = CullingViewKind::Editor;
	viewContext.cullingViewInstanceID = 0;
	m_renderSystem->PrepareRenderPacketView(viewContext);

	GraphicsContext* graphics = m_context->renderer->GetGraphicsContext();
	m_context->imgui->SetViewProjectionMatrix(viewContext.viewMatrix, viewContext.projectionMatrix);
	gBufferPass->Execute(viewContext);
	shadowMapPass->Execute(viewContext);
	graphics->SetCameraPosition(viewContext.CameraPosition);
	graphics->SetViewMatrix(viewContext.viewMatrix);
	graphics->SetProjectionMatrix(viewContext.projectionMatrix);
	lightingPass->SetTextureSlot(gBufferPass, shadowMapPass, graphics);
	lightingPass->Execute(viewContext);
	forwardPass->SetInputs(lightingPass, gBufferPass, shadowMapPass);
	forwardPass->Execute(viewContext);
	ID3D11ShaderResourceView* initialSRV = lightingPass->pRenderTarget->srv.Get();
	ID3D11RenderTargetView** initialRTV = lightingPass->pRenderTarget->rtv.GetAddressOf();
	postEffectPass->SetInputs(initialSRV, initialRTV, gBufferPass);
	postEffectPass->Execute(viewContext);
	overlayUIPass->SetInputs(postEffectPass->resultRtv, lightingPass->pRenderTarget);
	overlayUIPass->Execute(viewContext);
	graphics->GetDeviceContext()->OMSetRenderTargets(1, postEffectPass->resultRtv, gBufferPass->pDepthTarget->dsv.Get());
	physXDebugPass->Execute(viewContext);
	ID3D11RenderTargetView* nullRTV[1] = {nullptr};
	graphics->GetDeviceContext()->OMSetRenderTargets(1, nullRTV, nullptr);
	result = postEffectPass->resultSrv;
}
