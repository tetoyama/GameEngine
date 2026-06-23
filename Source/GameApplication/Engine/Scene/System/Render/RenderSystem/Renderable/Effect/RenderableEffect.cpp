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
#include "../../RenderPacket/RenderPacketTransformDX11.h"
#include <Component/EffectComponent.h>

void RenderableEffect::Execute(const RenderPassContext& ctx, const RenderPacket& packet){
	SceneContext* sceneContext = packet.bindings.sceneContext;
	const Entity& entity = packet.entity;
	if(!sceneContext) return;

	EffectComponent* effect = packet.bindings.effect;
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
