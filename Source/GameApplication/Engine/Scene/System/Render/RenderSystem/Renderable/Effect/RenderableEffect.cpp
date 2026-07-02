// =======================================================================
// 
// RenderableEffect.cpp
// 
// =======================================================================
#include "RenderableEffect.h"
#include "Backends/convertMatrix.h"
#include "Graphics/graphicsContext.h"
#include "scene.h"
#include "sceneManager.h"
#include "../../RenderPass/RenderPassContext.h"
#include <Component/EffectComponent.h>

void RenderableEffect::Execute(const RenderPassContext& ctx, const RenderPacket& packet){
	SceneContext* sceneContext = packet.bindings.sceneContext;
	if(!sceneContext) return;

	EffectComponent* effect = packet.bindings.effect;
	if(!effect) return;

	Effekseer::Matrix44 projection = ConvertXMMATRIXToMatrix44(ctx.projectionMatrix);
	Effekseer::Matrix44 view = ConvertXMMATRIXToMatrix44(ctx.viewMatrix);
	auto renderer = m_context->graphics->GetEffectRenderer();
	renderer->SetProjectionMatrix(projection);
	renderer->SetCameraMatrix(view);
	renderer->BeginRendering();
	m_context->graphics->GetEffectManager()->DrawHandle(effect->m_Handle);
	renderer->EndRendering();
}
