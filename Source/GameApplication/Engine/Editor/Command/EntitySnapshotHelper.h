// =======================================================================
//
// EntitySnapshotHelper.h
//
// =======================================================================
#pragma once

#include "Entity/Entity.h"
#include "Scene/scene.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/followComponent.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace EntityCommandHelper {

inline void SetParent(Entity child, Entity newParent, SceneContext* context){
	if(!context || !context->component) return;

	TransformComponent* childTransform =
		context->component->GetComponent<TransformComponent>(child);
	if(!childTransform) return;

	if(childTransform->parent){
		if(TransformComponent* oldParent =
			context->component->GetComponent<TransformComponent>(
				childTransform->parent
			)){
			auto& children = oldParent->children;
			children.erase(
				std::remove(children.begin(), children.end(), child),
				children.end()
			);
		}
	}

	childTransform->parent = newParent;
	if(newParent){
		if(TransformComponent* parentTransform =
			context->component->GetComponent<TransformComponent>(newParent)){
			if(std::find(
				parentTransform->children.begin(),
				parentTransform->children.end(),
				child
			) == parentTransform->children.end()){
				parentTransform->children.push_back(child);
			}
		}
	}
}

struct EntitySnapshot {
	Entity entity{};
	std::vector<std::pair<std::string, YAML::Node>> components;
};

using EntityRestoreMap = std::unordered_map<uint32_t, Entity>;

inline void SnapshotRecursive(
	Entity entity,
	SceneContext* context,
	std::vector<EntitySnapshot>& snapshots
){
	if(!context || !context->component) return;

	EntitySnapshot snapshot;
	snapshot.entity = entity;

	for(ComponentView component :
		context->component->GetAllComponentsOfEntitySorted(entity)){
		const std::string name =
			context->component->GetComponentName(component);
		if(name.empty()) continue;

		snapshot.components.emplace_back(
			name,
			context->component->EncodeComponent(component)
		);
	}
	snapshots.push_back(std::move(snapshot));

	if(TransformComponent* transform =
		context->component->GetComponent<TransformComponent>(entity)){
		for(Entity child : transform->children){
			SnapshotRecursive(child, context, snapshots);
		}
	}
}

inline void DestroyRecursive(Entity entity, SceneContext* context){
	if(!context || !context->entity ||
		!context->entity->IsAlive(entity)){
		return;
	}

	TransformComponent* transform =
		context->component->GetComponent<TransformComponent>(entity);
	const std::vector<Entity> children = transform
		? transform->children
		: std::vector<Entity>{};

	for(Entity child : children){
		DestroyRecursive(child, context);
	}

	SetParent(entity, {}, context);
	context->component->OnEntityDestroyed(entity);
	context->entity->Destroy(entity);
}

inline EntityRestoreMap RestoreAll(
	const std::vector<EntitySnapshot>& snapshots,
	SceneContext* context
){
	EntityRestoreMap restoredEntities;
	if(!context || !context->entity || !context->component){
		return restoredEntities;
	}

	restoredEntities.reserve(snapshots.size());

	for(const EntitySnapshot& snapshot : snapshots){
		const Entity restored = context->entity->CreateID(snapshot.entity);
		if(restored){
			restoredEntities[snapshot.entity.GetIndex()] = restored;
		}
	}

	for(const EntitySnapshot& snapshot : snapshots){
		auto entityIterator =
			restoredEntities.find(snapshot.entity.GetIndex());
		if(entityIterator == restoredEntities.end()) continue;

		for(const auto& [name, node] : snapshot.components){
			context->component->CreateFromYAML(
				name,
				entityIterator->second,
				node
			);
		}
	}

	const auto remapReference =
		[&](Entity serializedReference) -> Entity {
			const uint32_t index = serializedReference.GetIndex();
			if(index == 0) return {};

			if(auto iterator = restoredEntities.find(index);
				iterator != restoredEntities.end()){
				return iterator->second;
			}
			return context->entity->Resolve(index);
		};

	for(const auto& [oldIndex, restoredEntity] : restoredEntities){
		(void)oldIndex;
		if(TransformComponent* transform =
			context->component->GetComponent<TransformComponent>(restoredEntity)){
			transform->parent = remapReference(transform->parent);
			transform->children.clear();
		}

		if(FollowComponent* follow =
			context->component->GetComponent<FollowComponent>(restoredEntity)){
			follow->targetEntity = remapReference(follow->targetEntity);
		}
	}

	for(const auto& [oldIndex, restoredEntity] : restoredEntities){
		(void)oldIndex;
		TransformComponent* transform =
			context->component->GetComponent<TransformComponent>(restoredEntity);
		if(!transform || !transform->parent) continue;

		TransformComponent* parentTransform =
			context->component->GetComponent<TransformComponent>(
				transform->parent
			);
		if(!parentTransform) continue;

		if(std::find(
			parentTransform->children.begin(),
			parentTransform->children.end(),
			restoredEntity
		) == parentTransform->children.end()){
			parentTransform->children.push_back(restoredEntity);
		}
	}

	return restoredEntities;
}

} // namespace EntityCommandHelper
