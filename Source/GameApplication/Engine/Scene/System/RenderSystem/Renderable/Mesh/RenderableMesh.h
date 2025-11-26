#pragma once
#include "../IRenderable.h"
class ComponentRegistry;

struct RenderableContext;
struct SceneManagerContext;
class RenderableMesh: public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override{}
	void Finalize() override{}

	void Execute(
		const RenderableContext& ctx,
		SceneContext* sceneContext,
		const Entity& entity
	)override;
};
