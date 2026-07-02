// =======================================================================
// 
// EntityRef.h
// 
// =======================================================================
#pragma once
#include <cstdint>

#include "../Entity/Entity.h"
#include "scene.h"
#include "Registry/entityRegistry.h"

// エンティティへの安全なリファレンス。
// SceneManagerのメソッドを直接呼ばず、SceneContextに登録された解決コールバックを使う。
// これによりScript DLLなど別モジュールから利用しても外部シンボルを要求しない。
struct EntityRef {
	EntityRef() = default;

	EntityRef(Entity e, SceneContext* ctx)
		: m_entity(e) {
		if(!ctx || ctx->contextID == 0 || !ctx->resolver){
			return;
		}

		m_contextID = ctx->contextID;
		m_resolverOwner = ctx->resolverOwner;
		m_resolver = ctx->resolver;
	}

	bool IsValid() const {
		SceneContext* context = ResolveContext();
		if(!context || !context->entity) return false;
		return context->entity->IsAlive(m_entity);
	}

	Entity Get() const {
		return IsValid() ? m_entity : Entity(0,0);
	}

	Entity GetEntityID() const { return m_entity; }

	SceneContext* GetScene() const { return ResolveContext(); }

	uint32_t GetContextID() const { return m_contextID; }

	explicit operator bool() const {
		return IsValid();
	}

	bool operator==(const EntityRef& other) const {
		return m_entity == other.m_entity &&
			m_resolverOwner == other.m_resolverOwner &&
			m_contextID == other.m_contextID;
	}

	bool operator!=(const EntityRef& other) const {
		return !(*this == other);
	}

private:
	SceneContext* ResolveContext() const {
		if(!m_resolver || !m_resolverOwner || m_contextID == 0){
			return nullptr;
		}
		return m_resolver(m_resolverOwner, m_contextID);
	}

	Entity m_entity = 0;
	void* m_resolverOwner = nullptr;
	SceneContextResolver m_resolver = nullptr;
	uint32_t m_contextID = 0;
};
