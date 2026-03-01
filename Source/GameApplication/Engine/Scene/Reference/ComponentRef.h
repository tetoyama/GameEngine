#pragma once
#include "EntityRef.h"
#include "../Registry/componentRegistry.h"

// コンポーネントへの安全なリファレンス
// エンティティの有効性チェックは内包する EntityRef に委譲する
// SceneContext と EntityID の組で識別するため、マルチシーン環境でも安全に扱える
template<typename T>
struct ComponentRef {
	ComponentRef() = default;

	ComponentRef(Entity e, SceneContext* ctx)
		: m_entityRef(e, ctx) {}

	// EntityRef から直接構築する
	explicit ComponentRef(EntityRef ref)
		: m_entityRef(ref) {}

	// リファレンスが有効かどうかを確認する（エンティティが生存中かつコンポーネントが存在する）
	bool IsValid() const {
		if (!m_entityRef.IsValid()) return false;
		SceneContext* ctx = m_entityRef.GetScene();
		if (!ctx->component) return false;
		return ctx->component->GetComponent<T>(m_entityRef.GetEntityID()) != nullptr;
	}

	// コンポーネントのポインタを取得する（無効な場合は nullptr を返す）
	T* Get() const {
		if (!m_entityRef.IsValid()) return nullptr;
		SceneContext* ctx = m_entityRef.GetScene();
		if (!ctx->component) return nullptr;
		return ctx->component->GetComponent<T>(m_entityRef.GetEntityID());
	}

	T* operator->() const {
		return Get();
	}

	explicit operator bool() const {
		return IsValid();
	}

	// 生の Entity ID を取得する（有効性チェックなし）
	Entity GetEntityID() const { return m_entityRef.GetEntityID(); }

	// このリファレンスが属するシーンコンテキストを返す
	SceneContext* GetScene() const { return m_entityRef.GetScene(); }

	// 内包する EntityRef を返す
	const EntityRef& GetEntityRef() const { return m_entityRef; }

	// 内包する EntityRef が一致するときのみ等値とみなす
	bool operator==(const ComponentRef& other) const {
		return m_entityRef == other.m_entityRef;
	}

	bool operator!=(const ComponentRef& other) const {
		return !(*this == other);
	}

private:
	EntityRef m_entityRef;
};
