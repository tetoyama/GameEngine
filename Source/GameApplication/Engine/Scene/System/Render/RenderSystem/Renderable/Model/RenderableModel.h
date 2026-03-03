// =======================================================================
// 
// RenderableModel.h
// 
// =======================================================================
#pragma once
#include <d3d11.h>
#include "../IRenderable.h"

class ComponentRegistry;

struct RenderPassContext;
struct SceneManagerContext;

class RenderableModel :public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override{}
	void Finalize() override{}

	void Execute(
		const RenderPassContext& ctx,
		SceneContext* sceneContext,
		const Entity& entity
	) override;
};
