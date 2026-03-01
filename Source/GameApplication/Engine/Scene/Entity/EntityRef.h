#pragma once
#include "Entity.h"
#include "scene.h"
#include "Registry/entityRegistry.h"

// エンティティへの安全なリファレンス
// SceneContext・EntityID・世代番号の3つで識別する
//   - SceneContext で所属シーンを区別する（マルチシーン対応）
//   - 世代番号で EntityID の使いまわしを検出する
//     例: entity 5 を破棄 → ID 5 を再利用して新エンティティを作成した場合、
//         古いリファレンスの世代番号が一致しないため IsValid() が false を返す
struct EntityRef {
	EntityRef() = default;

	EntityRef(Entity e, SceneContext* ctx)
		: m_entity(e)
		, m_context(ctx)
		, m_generation(ctx && ctx->entity ? ctx->entity->GetGeneration(e) : 0)
	{}

	// リファレンスが有効かどうかを確認する
	// エンティティが生存中かつ世代番号が一致する場合のみ true を返す
	bool IsValid() const {
		if (!m_context || !m_context->entity) return false;
		return m_context->entity->IsAliveWithGeneration(m_entity, m_generation);
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

	// SceneContext・EntityID・世代番号の3つが全て一致する場合のみ等値とみなす
	bool operator==(const EntityRef& other) const {
		return m_entity     == other.m_entity
			&& m_generation == other.m_generation
			&& m_context    == other.m_context;
	}

	bool operator!=(const EntityRef& other) const {
		return !(*this == other);
	}

private:
	Entity       m_entity     = 0;
	uint32_t     m_generation = 0;
	SceneContext* m_context   = nullptr;
};
