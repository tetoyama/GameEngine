#pragma once
#include "Entity/Entity.h"
#include "Interface/IComponent.h"
#include <bitset>

#include <backends/yaml-cpp/yaml.h>
using ComponentTypeID = uint32_t;
constexpr size_t MAX_COMPONENTS = 64;  // ïKóvÇ…âûÇ∂Çƒí≤êÆ
using ComponentMask = std::bitset<MAX_COMPONENTS>;

struct IComponentStorage {
	virtual ~IComponentStorage() = default;
	virtual void Remove(Entity e) = 0;
	virtual std::vector<Entity> GetEntityList() const = 0;
	virtual IComponent* GetEntityComponent(Entity) = 0;
};

template<typename T>
class ArchetypeStorage: public IComponentStorage {
	static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from IComponent");

public:
	void Add(Entity e, T component){
		if(m_indexMap.contains(e)) return;
		size_t idx = m_components.size();
		m_indexMap[e] = idx;
		m_components.emplace_back(std::move(component));
		m_entities.emplace_back(e);
	}

	T* Get(Entity e){
		auto it = m_indexMap.find(e);
		return (it != m_indexMap.end()) ? &m_components[it->second] : nullptr;
	}

	void Remove(Entity e) override{
		auto it = m_indexMap.find(e);
		if(it == m_indexMap.end()) return;
		size_t idx = it->second;
		size_t last = m_components.size() - 1;

		if(idx != last){
			m_components[idx] = std::move(m_components[last]);
			Entity movedEntity = m_entities[last];
			m_entities[idx] = movedEntity;
			m_indexMap[movedEntity] = idx;
		}

		m_components.pop_back();
		m_entities.pop_back();
		m_indexMap.erase(it);
	}

	std::vector<Entity> GetEntityList() const override{
		return m_entities;
	}

	IComponent* GetEntityComponent(Entity e) {
		return Get(e);
	}

private:
	std::unordered_map<Entity, size_t> m_indexMap;
	std::vector<T> m_components;
	std::vector<Entity> m_entities;
};

template<typename T>
class SparseStorage: public IComponentStorage {
	static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from IComponent");

public:
	void Add(Entity e, T component){
		m_map.emplace(e, std::move(component));
	}

	T* Get(Entity e){
		auto it = m_map.find(e);
		return (it != m_map.end()) ? &it->second : nullptr;
	}

	void Remove(Entity e) override{
		m_map.erase(e);
	}

	std::vector<Entity> GetEntityList() const override{
		std::vector<Entity> result;
		result.reserve(m_map.size());
		for(const auto& [entity, _] : m_map)
			result.push_back(entity);
		return result;
	}

	IComponent* GetEntityComponent(Entity e) {
		return Get(e);
	}

private:
	std::unordered_map<Entity, T> m_map;
};
