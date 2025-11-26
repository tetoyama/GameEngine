#pragma once
#include "GameApplication/Engine/Scene/Entity/Entity.h"

struct RenderableContext;
struct SceneManagerContext;
struct SceneContext;

class ComponentRegistry;

class IRenderable {
public:
	virtual ~IRenderable(){}

	virtual void Initialize(SceneManagerContext* context) = 0;
	virtual void Finalize() = 0;

	virtual void Execute(
		const RenderableContext& ctx,
		SceneContext* sceneContext,
		const Entity& entity
	) = 0;
};
