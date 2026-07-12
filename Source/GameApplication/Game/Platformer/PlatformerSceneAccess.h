#pragma once

#include "Engine/Scene/scene.h"
#include "Engine/Scene/Reference/ComponentRef.h"
#include "Engine/Scene/Reference/EntityRef.h"
#include "Engine/Scene/Registry/componentRegistry.h"
#include "Engine/Scene/System/Physic/physicSystem.h"
#include "Engine/Scene/sceneManager.h"

// Game-facing access boundary. Platformer scripts use this helper instead of
// scattering SceneContext registry/service traversal throughout gameplay code.
class PlatformerSceneAccess {
public:
	template<typename T>
	static ComponentRef<T> Get(EntityRef entity) {
		if(!entity.IsValid()) return {};
		return ComponentRef<T>(entity);
	}

	template<typename T>
	static ComponentRef<T> Get(Entity entity, SceneContext* context) {
		if(!context || !entity) return {};
		return ComponentRef<T>(entity, context);
	}

	template<typename T>
	static ComponentRef<T> FindFirst(SceneContext* context) {
		if(!context || !context->component) return {};
		const auto entities = context->component->FindEntitiesWithComponent<T>();
		if(entities.empty()) return {};
		return ComponentRef<T>(entities.front(), context);
	}

	template<typename T>
	static EntityRef FindFirstEntity(SceneContext* context) {
		if(!context || !context->component) return {};
		const auto entities = context->component->FindEntitiesWithComponent<T>();
		if(entities.empty()) return {};
		return EntityRef(entities.front(), context);
	}

	static PhysicSystem* Physics(SceneContext* context) {
		if(!context || !context->system) return nullptr;
		return context->system->GetSystem<PhysicSystem>();
	}

	static AudioContext* Audio(SceneContext* context) {
		return context && context->manager ? context->manager->audio : nullptr;
	}

	static ResourceService* Resources(SceneContext* context) {
		return context && context->manager ? context->manager->resource : nullptr;
	}

	static bool IsAlive(const EntityRef& entity) {
		return entity.IsValid();
	}
};
