// =======================================================================
// 
// EntitySnapshotHelper.h
// 
// =======================================================================
#pragma once

#include "Entity/Entity.h"
#include "Scene/scene.h"
#include "Scene/Interface/IComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/followComponent.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace EntityCommandHelper {

inline void SetParent(Entity child, Entity newParent, SceneContext* ctx) {
	auto* childT = ctx->component->GetComponent<TransformComponent>(child);
	if(!childT) return;

	if(childT->parent != 0){
		auto* oldParentT = ctx->component->GetComponent<TransformComponent>(childT->parent);
		if(oldParentT){
			auto& children = oldParentT->children;
			children.erase(std::remove(children.begin(), children.end(), child), children.end());
		}
	}

	childT->parent = newParent;

	if(newParent != 0){
		auto* newParentT = ctx->component->GetComponent<TransformComponent>(newParent);
		if(newParentT){
			newParentT->children.push_back(child);
		}
	}
}

struct EntitySnapshot {
	Entity entity;
	std::vector<std::pair<std::string, YAML::Node>> components;
};

using EntityRestoreMap = std::unordered_map<uint32_t, Entity>;

inline void SnapshotRecursive(
	Entity entity,
	SceneContext* ctx,
	std::vector<EntitySnapshot>& snapshots
) {
	EntitySnapshot snapshot;
	snapshot.entity = entity;

	const auto& idToName = ctx->component->GetComponentIDToNameMap();
	for(IComponent* component : ctx->component->GetAllComponentsOfEntitySorted(entity)){
		std::type_index typeIndex(typeid(*component));
		ComponentTypeID typeID = ctx->component->GetComponentIDByTypeIndex(typeIndex);
		if(typeID == static_cast<ComponentTypeID>(-1)) continue;

		auto nameIt = idToName.find(typeID);
		if(nameIt == idToName.end()) continue;

		snapshot.components.emplace_back(nameIt->second, component->encode());
	}
	snapshots.push_back(std::move(snapshot));

	auto* transform = ctx->component->GetComponent<TransformComponent>(entity);
	if(transform){
		for(Entity child : transform->children){
			SnapshotRecursive(child, ctx, snapshots);
		}
	}
}

inline void DestroyRecursive(Entity entity, SceneContext* ctx) {
	if(!ctx->entity->IsAlive(entity)) return;

	auto* transform = ctx->component->GetComponent<TransformComponent>(entity);
	const std::vector<Entity> children =
		transform ? transform->children : std::vector<Entity>{};

	for(Entity child : children){
		DestroyRecursive(child, ctx);
	}

	SetParent(entity, 0, ctx);
	ctx->component->OnEntityDestroyed(entity);
	ctx->entity->Destroy(entity);
}

// Snapshot内のEntityを同じindex・現在generationで復元し、
// 旧indexから新しい実行時Entityへの対応表を返す。
inline EntityRestoreMap RestoreAll(
	const std::vector<EntitySnapshot>& snapshots,
	SceneContext* ctx
) {
	EntityRestoreMap restoredEntities;
	restoredEntities.reserve(snapshots.size());

	// Pass 1: 全Entityを復元する。
	for(const EntitySnapshot& snapshot : snapshots){
		const Entity restored = ctx->entity->CreateID(snapshot.entity);
		if(restored){
			restoredEntities[snapshot.entity.GetIndex()] = restored;
		}
	}

	// Pass 2: 新しいgenerationのEntityへComponentを復元する。
	for(const EntitySnapshot& snapshot : snapshots){
		auto entityIt = restoredEntities.find(snapshot.entity.GetIndex());
		if(entityIt == restoredEntities.end()) continue;

		for(const auto& [name, node] : snapshot.components){
			ctx->component->CreateFromYAML(name, entityIt->second, node);
		}
	}

	const auto remapReference = [&](Entity serializedReference) -> Entity {
		const uint32_t index = serializedReference.GetIndex();
		if(index == 0) return {};

		auto restoredIt = restoredEntities.find(index);
		if(restoredIt != restoredEntities.end()){
			return restoredIt->second;
		}

		// Snapshot外の親や追従対象は、現在生存しているEntityへ接続する。
		return ctx->entity->Resolve(index);
	};

	// Pass 3: Component内のEntity参照を現在generationへ変換する。
	for(const auto& [oldIndex, restoredEntity] : restoredEntities){
		if(auto* transform = ctx->component->GetComponent<TransformComponent>(restoredEntity)){
			transform->parent = remapReference(transform->parent);
			transform->children.clear();
		}

		if(auto* follow = ctx->component->GetComponent<FollowComponent>(restoredEntity)){
			follow->targetEntity = remapReference(follow->targetEntity);
		}
	}

	// Pass 4: 親参照からchildrenを再構築する。
	for(const auto& [oldIndex, restoredEntity] : restoredEntities){
		auto* transform = ctx->component->GetComponent<TransformComponent>(restoredEntity);
		if(!transform || transform->parent == 0) continue;

		auto* parentTransform =
			ctx->component->GetComponent<TransformComponent>(transform->parent);
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
