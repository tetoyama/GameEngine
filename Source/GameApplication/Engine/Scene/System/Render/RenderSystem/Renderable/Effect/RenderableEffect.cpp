// =======================================================================
// 
// RenderableEffect.cpp
// 
// =======================================================================
#include "RenderableEffect.h"
#include "Backends/convertMatrix.h"
#include "Graphics/graphicsContext.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "Registry/componentRegistry.h"
#include "scene.h"
#include "sceneManager.h"
#include "../../RenderPass/RenderPassContext.h"
#include <Component/EffectComponent.h>

void RenderableEffect::Execute(const RenderPassContext& ctx, SceneContext* sceneContext, const Entity& entity) {

	EffectComponent* effect = sceneContext->component->GetComponent<EffectComponent>(entity);
	if(!effect){
		return;
	}
	Effekseer::Matrix44 effekseerProjectionMatrix = ConvertXMMATRIXToMatrix44(ctx.projectionMatrix);
	Effekseer::Matrix44 effekseerViewMatrix = ConvertXMMATRIXToMatrix44(ctx.viewMatrix);

	m_context->graphics->GetEffectRenderer()->SetProjectionMatrix(effekseerProjectionMatrix);
	m_context->graphics->GetEffectRenderer()->SetCameraMatrix(effekseerViewMatrix);

	m_context->graphics->GetEffectRenderer()->BeginRendering();
	m_context->graphics->GetEffectManager()->DrawHandle(effect->m_Handle);
	m_context->graphics->GetEffectRenderer()->EndRendering();
}
