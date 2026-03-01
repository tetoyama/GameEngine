#pragma once
#include "Entity.h"
#include "Registry/componentRegistry.h"

// コンポーネントへの安全なリファレンス
// SceneContext・EntityID・世代番号の3つで識別する
//   - SceneContext で所属シーンを区別する（マルチシーン対応）
//   - 世代番号で EntityID の使いまわしを検出する
template<typename T>
struct ComponentRef {
	ComponentRef() = default;

	ComponentRef(Entity e, SceneContext* ctx)
		: m_entity(e)
		, m_context(ctx)
		, m_generation(ctx && ctx->entity ? ctx->entity->GetGeneration(e) : 0)
	{}

	// リファレンスが有効かどうかを確認する
	// エンティティが生存中・世代番号が一致・コンポーネントが存在する場合のみ true を返す
	bool IsValid() const {
		if (!m_context || !m_context->entity || !m_context->component) return false;
		if (!m_context->entity->IsAliveWithGeneration(m_entity, m_generation)) return false;
		return m_context->component->GetComponent<T>(m_entity) != nullptr;
	}

	// コンポーネントのポインタを取得する（無効な場合は nullptr を返す）
	T* Get() const {
		if (!m_context || !m_context->entity || !m_context->component) return nullptr;
		if (!m_context->entity->IsAliveWithGeneration(m_entity, m_generation)) return nullptr;
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

	// SceneContext・EntityID・世代番号の3つが全て一致する場合のみ等値とみなす
	bool operator==(const ComponentRef& other) const {
		return m_entity     == other.m_entity
			&& m_generation == other.m_generation
			&& m_context    == other.m_context;
	}

	bool operator!=(const ComponentRef& other) const {
		return !(*this == other);
	}

private:
	Entity        m_entity     = 0;
	uint32_t      m_generation = 0;
	SceneContext*  m_context   = nullptr;
};
