// =======================================================================
// 
// IComponentStorage.h
// 
// =======================================================================
#pragma once

#include "Entity/Entity.h"
#include "Interface/IComponent.h"
#include <bitset>
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <backends/yaml-cpp/yaml.h>

using ComponentTypeID = uint32_t;

constexpr size_t MAX_COMPONENTS = 64;
using ComponentMask = std::bitset<MAX_COMPONENTS>;

struct IComponentStorage {
	virtual ~IComponentStorage() = default;

	virtual void Remove(Entity entity) = 0;
	virtual std::vector<Entity> GetEntityList() const = 0;
	virtual IComponent* GetEntityComponent(Entity entity) = 0;

	// Componentの削除・再追加を識別するインスタンス世代。
	virtual uint32_t GetComponentGeneration(Entity entity) const = 0;
};

template<typename T>
class ArchetypeStorage: public IComponentStorage {
	static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from IComponent");

public:
	void Add(Entity entity, T component) {
		if(m_indexMap.contains(entity)) return;

		const size_t index = m_components.size();
		m_indexMap[entity] = index;
		m_components.emplace_back(std::move(component));
		m_entities.emplace_back(entity);

		// Remove後の再追加では、Remove時に進めた世代を維持する。
		m_generations.try_emplace(entity, 0);
	}

	T* Get(Entity entity) {
		auto it = m_indexMap.find(entity);
		return it != m_indexMap.end() ? &m_components[it->second] : nullptr;
	}

	void Remove(Entity entity) override {
		auto it = m_indexMap.find(entity);
		if(it == m_indexMap.end()) return;

		const size_t index = it->second;
		const size_t last = m_components.size() - 1;

		if(index != last){
			m_components[index] = std::move(m_components[last]);
			const Entity movedEntity = m_entities[last];
			m_entities[index] = movedEntity;
			m_indexMap[movedEntity] = index;
		}

		m_components.pop_back();
		m_entities.pop_back();
		m_indexMap.erase(it);
		IncrementGeneration(entity);
	}

	std::vector<Entity> GetEntityList() const override {
		return m_entities;
	}

	IComponent* GetEntityComponent(Entity entity) override {
		return Get(entity);
	}

	uint32_t GetComponentGeneration(Entity entity) const override {
		auto it = m_generations.find(entity);
		return it != m_generations.end() ? it->second : 0;
	}

private:
	void IncrementGeneration(Entity entity) {
		uint32_t& generation = m_generations[entity];
		++generation;
		if(generation == 0) ++generation;
	}

	std::unordered_map<Entity, size_t> m_indexMap;
	std::vector<T> m_components;
	std::vector<Entity> m_entities;
	std::unordered_map<Entity, uint32_t> m_generations;
};

template<typename T>
class SparseStorage: public IComponentStorage {
	static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from IComponent");

public:
	// 既に存在する場合は既存Componentを維持する。
	void Add(Entity entity, T component) {
		if(m_map.contains(entity)) return;

		m_map.emplace(entity, std::move(component));
		// Remove後の再追加では、Remove時に進めた世代を維持する。
		m_generations.try_emplace(entity, 0);
	}

	T* Get(Entity entity) {
		auto it = m_map.find(entity);
		return it != m_map.end() ? &it->second : nullptr;
	}

	void Remove(Entity entity) override {
		auto it = m_map.find(entity);
		if(it == m_map.end()) return;

		m_map.erase(it);
		IncrementGeneration(entity);
	}

	std::vector<Entity> GetEntityList() const override {
		std::vector<Entity> result;
		result.reserve(m_map.size());
		for(const auto& [entity, component] : m_map){
			result.push_back(entity);
		}
		return result;
	}

	IComponent* GetEntityComponent(Entity entity) override {
		return Get(entity);
	}

	uint32_t GetComponentGeneration(Entity entity) const override {
		auto it = m_generations.find(entity);
		return it != m_generations.end() ? it->second : 0;
	}

private:
	void IncrementGeneration(Entity entity) {
		uint32_t& generation = m_generations[entity];
		++generation;
		if(generation == 0) ++generation;
	}

	std::unordered_map<Entity, T> m_map;
	std::unordered_map<Entity, uint32_t> m_generations;
};
