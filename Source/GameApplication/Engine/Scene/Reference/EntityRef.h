// =======================================================================
// 
// EntityRef.h
// 
// =======================================================================
#pragma once
#include <cstdint>

#include "../Entity/Entity.h"
#include "scene.h"
#include "sceneManager.h"
#include "Registry/entityRegistry.h"

// エンティティへの安全なリファレンス
// 生のSceneContextポインタは保持せず、SceneManagerとContext IDから毎回解決する。
// Scene破棄時にContext登録が解除されるため、破棄後の参照は安全に無効化される。
struct EntityRef {
	EntityRef() = default;

	EntityRef(Entity e, SceneContext* ctx)
		: m_entity(e) {
		if(!ctx || !ctx->manager || !ctx->manager->sceneManager){
			return;
		}

		m_sceneManager = ctx->manager->sceneManager;
		m_contextID = m_sceneManager->GetIDFromContext(ctx);
	}

	// リファレンスが有効かどうかを確認する。
	bool IsValid() const {
		SceneContext* context = ResolveContext();
		if(!context || !context->entity) return false;
		return context->entity->IsAlive(m_entity);
	}

	// エンティティIDを取得する（無効な場合は0を返す）。
	Entity Get() const {
		return IsValid() ? m_entity : 0;
	}

	// 生のEntity IDを取得する（有効性チェックなし）。
	Entity GetEntityID() const { return m_entity; }

	// 現在有効なSceneContextをIDから解決する。
	// Sceneが破棄済みならnullptrを返す。
	SceneContext* GetScene() const { return ResolveContext(); }

	uint32_t GetContextID() const { return m_contextID; }

	explicit operator bool() const {
		return IsValid();
	}

	bool operator==(const EntityRef& other) const {
		return m_entity == other.m_entity &&
			m_sceneManager == other.m_sceneManager &&
			m_contextID == other.m_contextID;
	}

	bool operator!=(const EntityRef& other) const {
		return !(*this == other);
	}

private:
	SceneContext* ResolveContext() const {
		if(!m_sceneManager || m_contextID == 0) return nullptr;
		return m_sceneManager->GetContextFromID(m_contextID);
	}

	Entity m_entity = 0;
	SceneManager* m_sceneManager = nullptr;
	uint32_t m_contextID = 0;
};
