#pragma once
#include "Entity.h"
#include "Registry/componentRegistry.h"

// コンポーネントへの安全なリファレンス
// SceneContext と EntityID の組で識別するため、マルチシーン環境でも安全に扱える
// エンティティが破棄された後でも安全にアクセスできる
template<typename T>
struct ComponentRef {
	ComponentRef() = default;

	ComponentRef(Entity e, SceneContext* ctx)
		: m_entity(e), m_context(ctx) {}

	// リファレンスが有効かどうかを確認する（エンティティが生存中かつコンポーネントが存在する）
	bool IsValid() const {
		if (!m_context || !m_context->entity || !m_context->component) return false;
		if (!m_context->entity->IsAlive(m_entity)) return false;
		return m_context->component->GetComponent<T>(m_entity) != nullptr;
	}

	// コンポーネントのポインタを取得する（無効な場合は nullptr を返す）
	T* Get() const {
		if (!m_context || !m_context->entity || !m_context->component) return nullptr;
		if (!m_context->entity->IsAlive(m_entity)) return nullptr;
		return m_context->component->GetComponent<T>(m_entity);
	}

	T* operator->() const {
		return Get();
	}

	explicit operator bool() const {
		return IsValid();
	}

	// 生の Entity ID を取得する（有効性チェックなし）
	Entity GetEntityID() const { return m_entity; }

	// このリファレンスが属するシーンコンテキストを返す
	SceneContext* GetScene() const { return m_context; }

	// SceneContext と EntityID の両方が一致するときのみ等値とみなす
	bool operator==(const ComponentRef& other) const {
		return m_entity == other.m_entity && m_context == other.m_context;
	}

	bool operator!=(const ComponentRef& other) const {
		return !(*this == other);
	}

private:
	Entity m_entity = 0;
	SceneContext* m_context = nullptr;
};
