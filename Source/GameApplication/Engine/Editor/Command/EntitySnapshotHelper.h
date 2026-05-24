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
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <typeindex>
#include <algorithm>

namespace m_EntityCommandHelper{

// -----------------------------------------------------------------------
// 親子関係の設定（SetParentCommand / EntityCreateCommand などでも使用）
// -----------------------------------------------------------------------
inline void SetParent(Entity child, Entity newParent, SceneContext* ctx) {
	auto* childT = ctx->component->GetComponent<TransformComponent>(child);
	if (!childT) return;

	// 旧親の children から除去
	if (childT->parent != 0) {
		auto* oldParentT = ctx->component->GetComponent<TransformComponent>(childT->parent);
		if (oldParentT) {
			auto& ch = oldParentT->children;
			ch.erase(std::remove(ch.begin(), ch.end(), child), ch.end());
		}
	}

	childT->parent = newParent;

	// 新親の children に追加
	if (newParent != 0) {
		auto* newParentT = ctx->component->GetComponent<TransformComponent>(newParent);
		if (newParentT) {
			newParentT->children.push_back(child);
		}
	}
}

// -----------------------------------------------------------------------
// エンティティとそのコンポーネント群のスナップショット
// -----------------------------------------------------------------------
struct EntitySnapshot {
	Entity entity;
	std::vector<std::pair<std::string, YAML::Node>> components;
};

// エンティティとその子孫を再帰的にスナップショット（親→子の順）
inline void SnapshotRecursive(Entity e, SceneContext* ctx, std::vector<EntitySnapshot>& snapshots) {
	EntitySnapshot snap;
	snap.entity = e;

	const auto& idToName = ctx->component->GetComponentIDToNameMap();
	for (IComponent* comp : ctx->component->GetAllComponentsOfEntitySorted(e)) {
		std::type_index ti(typeid(*comp));
		ComponentTypeID typeID = ctx->component->GetComponentIDByTypeIndex(ti);
		if (typeID == static_cast<ComponentTypeID>(-1)) continue;
		auto nameIt = idToName.find(typeID);
		if (nameIt == idToName.end()) continue;
		snap.components.emplace_back(nameIt->second, comp->encode());
	}
	snapshots.push_back(std::move(snap));

	auto* t = ctx->component->GetComponent<TransformComponent>(e);
	if (t) {
		for (Entity child : t->children)
			SnapshotRecursive(child, ctx, snapshots);
	}
}

// -----------------------------------------------------------------------
// エンティティとその子孫を再帰的に削除する
// -----------------------------------------------------------------------
inline void DestroyRecursive(Entity e, SceneContext* ctx) {
	if (!ctx->entity->IsAlive(e)) return;

	auto* t = ctx->component->GetComponent<TransformComponent>(e);
	std::vector<Entity> children = t ? t->children : std::vector<Entity>{};
	for (Entity child : children)
		DestroyRecursive(child, ctx);

	SetParent(e, 0, ctx);
	ctx->component->OnEntityDestroyed(e);
	ctx->entity->Destroy(e);
}

// -----------------------------------------------------------------------
// スナップショットからエンティティを復元する（3パス）
// -----------------------------------------------------------------------
inline void RestoreAll(const std::vector<EntitySnapshot>& snapshots, SceneContext* ctx) {
	// Pass 1: エンティティを作成（同一 ID で再生成）
	for (const auto& snap : snapshots) {
		if (!ctx->entity->IsAlive(snap.entity))
			ctx->entity->CreateID(snap.entity);
	}
	// Pass 2: コンポーネントを復元（TransformComponent の parent フィールドも含む）
	for (const auto& snap : snapshots) {
		for (const auto& [name, node] : snap.components)
			ctx->component->CreateFromYAML(name, snap.entity, node);
	}
	// Pass 3: TransformComponent の children リストを親子関係から再構築
	for (const auto& snap : snapshots) {
		auto* t = ctx->component->GetComponent<TransformComponent>(snap.entity);
		if (!t || t->parent == 0) continue;
		auto* parentT = ctx->component->GetComponent<TransformComponent>(t->parent);
		if (!parentT) continue;
		if (std::find(parentT->children.begin(), parentT->children.end(), snap.entity)
			== parentT->children.end()) {
			parentT->children.push_back(snap.entity);
		}
	}
}

} // namespace EntityCommandHelper
