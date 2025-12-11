#pragma once
#include "../IRenderable.h"
class ComponentRegistry;

struct RenderableContext;
struct SceneManagerContext;
class RenderableWave : public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override {}
	void Finalize() override {}

	void Execute(
		const RenderPassContext& ctx,
		SceneContext* sceneContext,
		const Entity& entity
	)override;
};
