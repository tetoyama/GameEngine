// =======================================================================
// 
// ComponentRef.h
// 
// =======================================================================
#pragma once

#include <cassert>
#include <cstdint>

#include "EntityRef.h"
#include "../Registry/componentRegistry.h"

// Entity世代とComponentインスタンス世代の両方を検証する安全な参照。
// 返されたポインタは次の構造変更までの一時参照として扱う。
template<typename T>
struct ComponentRef {
	ComponentRef() = default;

	ComponentRef(Entity entity, SceneContext* context)
		: m_entityRef(entity, context) {
		CaptureGeneration();
	}

	explicit ComponentRef(EntityRef ref)
		: m_entityRef(ref) {
		CaptureGeneration();
	}

	bool IsValid() const {
		if(!m_hasGeneration || !m_entityRef.IsValid()) return false;

		SceneContext* context = m_entityRef.GetScene();
		if(!context || !context->component) return false;

		const Entity entity = m_entityRef.GetEntityID();
		if(context->component->GetComponentGeneration<T>(entity) != m_componentGeneration){
			return false;
		}

		return context->component->GetComponent<T>(entity) != nullptr;
	}

	T* TryGet() const {
		if(!IsValid()) return nullptr;

		SceneContext* context = m_entityRef.GetScene();
		return context->component->GetComponent<T>(m_entityRef.GetEntityID());
	}

	// 既存コード互換。新規コードではTryGet()を優先する。
	T* Get() const {
		return TryGet();
	}

	T& GetChecked() const {
		T* component = TryGet();
		assert(component && "ComponentRef::GetChecked called on invalid reference");
		return *component;
	}

	T* operator->() const {
		return TryGet();
	}

	explicit operator bool() const {
		return IsValid();
	}

	void Reset() {
		m_entityRef = EntityRef{};
		m_componentGeneration = 0;
		m_hasGeneration = false;
	}

	Entity GetEntityID() const { return m_entityRef.GetEntityID(); }
	SceneContext* GetScene() const { return m_entityRef.GetScene(); }
	const EntityRef& GetEntityRef() const { return m_entityRef; }
	uint32_t GetComponentGeneration() const { return m_componentGeneration; }

	bool operator==(const ComponentRef& other) const {
		return m_entityRef == other.m_entityRef &&
			m_componentGeneration == other.m_componentGeneration &&
			m_hasGeneration == other.m_hasGeneration;
	}

	bool operator!=(const ComponentRef& other) const {
		return !(*this == other);
	}

private:
	void CaptureGeneration() {
		if(!m_entityRef.IsValid()) return;

		SceneContext* context = m_entityRef.GetScene();
		if(!context || !context->component) return;

		const Entity entity = m_entityRef.GetEntityID();
		if(!context->component->GetComponent<T>(entity)) return;

		m_componentGeneration =
			context->component->GetComponentGeneration<T>(entity);
		m_hasGeneration = true;
	}

	EntityRef m_entityRef;
	uint32_t m_componentGeneration = 0;
	bool m_hasGeneration = false;
};
