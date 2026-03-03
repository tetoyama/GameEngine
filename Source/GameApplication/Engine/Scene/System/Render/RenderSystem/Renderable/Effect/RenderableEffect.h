// =======================================================================
// 
// RenderableEffect.h
// 
// =======================================================================
#pragma once
#include "../IRenderable.h"
class ComponentRegistry;

struct RenderPassContext;
struct SceneManagerContext;
class RenderableEffect : public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override { m_context = context; }
	void Finalize() override {}

	void Execute(
		const RenderPassContext& ctx,
		SceneContext* sceneContext,
		const Entity& entity
	)override;

private:
	SceneManagerContext* m_context = nullptr;
};
