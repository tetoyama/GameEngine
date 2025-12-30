#include "EffectPass.h"
#include "Graphics/graphicsContext.h"
#include "Backends/convertMatrix.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "scene.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"

void EffectPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {
	m_renderSystem = renderSystem;
	m_context = context;
}

void EffectPass::Finalize() {}

void EffectPass::Execute(const RenderPassContext& ctx) {
	Effekseer::Matrix44 effekseerProjectionMatrix = ConvertXMMATRIXToMatrix44(ctx.projectionMatrix);
	Effekseer::Matrix44 effekseerViewMatrix = ConvertXMMATRIXToMatrix44(ctx.viewMatrix);

	m_context->graphics->GetEffectRenderer()->SetProjectionMatrix(effekseerProjectionMatrix);
	m_context->graphics->GetEffectRenderer()->SetCameraMatrix(effekseerViewMatrix);

	m_context->graphics->GetEffectRenderer()->BeginRendering();
	m_context->graphics->GetEffectManager()->Draw();
	m_context->graphics->GetEffectRenderer()->EndRendering();
}
