// =======================================================================
// 
// IRenderable.h
// 
// =======================================================================
#pragma once
#include "Scene/Entity/Entity.h"

struct RenderPassContext;
struct SceneManagerContext;
struct SceneContext;

class ComponentRegistry;

class IRenderable {
public:
	virtual ~IRenderable(){}

	virtual void Initialize(SceneManagerContext* context) = 0;
	virtual void Finalize() = 0;

	virtual void Execute(
		const RenderPassContext& ctx,
		SceneContext* sceneContext,
		const Entity& entity
	) = 0;
};
