// =======================================================================
// 
// EntityRef.h
// 
// =======================================================================
#pragma once
#include "../Entity/Entity.h"
#include "scene.h"
#include "Registry/entityRegistry.h"

// エンティティへの安全なリファレンス
// SceneContext と EntityID の組で識別するため、マルチシーン環境でも安全に扱える
// エンティティが破棄された後でも安全にアクセスできる
struct EntityRef {
	EntityRef() = default;

	EntityRef(Entity e, SceneContext* ctx)
		: m_entity(e), m_context(ctx) {}

	// リファレンスが有効かどうかを確認する
	bool IsValid() const {
		if (!m_context || !m_context->entity) return false;
		return m_context->entity->IsAlive(m_entity);
	}

	// エンティティIDを取得する（無効な場合は 0 を返す）
	Entity Get() const {
		return IsValid() ? m_entity : 0;
	}

	// 生の Entity ID を取得する（有効性チェックなし）
	Entity GetEntityID() const { return m_entity; }

	// このリファレンスが属するシーンコンテキストを返す
	SceneContext* GetScene() const { return m_context; }

	explicit operator bool() const {
		return IsValid();
	}

	// SceneContext と EntityID の両方が一致するときのみ等値とみなす
	bool operator==(const EntityRef& other) const {
		return m_entity == other.m_entity && m_context == other.m_context;
	}

	bool operator!=(const EntityRef& other) const {
		return !(*this == other);
	}

private:
	Entity m_entity = 0;
	SceneContext* m_context = nullptr;
};
