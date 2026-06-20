// =======================================================================
//
// DenseComponentPool.h
//
// Entity.indexをSparse Indexとして使用するdense component storage。
//
// =======================================================================
#pragma once

#include "Scene/Entity/Entity.h"
#include "Scene/Interface/IComponentStorage.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

template<typename T>
class DenseComponentPool final: public IComponentStorage {
	static_assert(
		std::is_base_of_v<IComponent, T>,
		"T must inherit from IComponent during migration"
	);
	static_assert(
		std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
		"DenseComponentPool requires movable components"
	);

public:
	static constexpr size_t InvalidDenseIndex =
		(std::numeric_limits<size_t>::max)();

	void Add(Entity entity, T component){
		if(!entity){
			return;
		}

		EnsureIndexCapacity(entity.GetIndex());
		const size_t existingIndex = m_sparseIndices[entity.GetIndex()];
		if(IsDenseIndexValid(existingIndex)){
			if(m_entities[existingIndex] == entity){
				return;
			}

			// 同じindexの旧世代Entityが残っていた場合は除去して整合性を戻す。
			RemoveAt(existingIndex);
		}

		const size_t denseIndex = m_components.size();
		m_components.emplace_back(std::move(component));
		m_entities.emplace_back(entity);
		m_sparseIndices[entity.GetIndex()] = denseIndex;
		IncrementStructureVersion();
	}

	T* Get(Entity entity){
		const size_t denseIndex = FindDenseIndex(entity);
		return IsDenseIndexValid(denseIndex)
			? &m_components[denseIndex]
			: nullptr;
	}

	const T* Get(Entity entity) const {
		const size_t denseIndex = FindDenseIndex(entity);
		return IsDenseIndexValid(denseIndex)
			? &m_components[denseIndex]
			: nullptr;
	}

	bool Contains(Entity entity) const {
		return IsDenseIndexValid(FindDenseIndex(entity));
	}

	void Remove(Entity entity) override {
		const size_t denseIndex = FindDenseIndex(entity);
		if(IsDenseIndexValid(denseIndex)){
			RemoveAt(denseIndex);
		}
	}

	void Reserve(size_t capacity){
		const size_t oldComponentCapacity = m_components.capacity();
		const size_t oldEntityCapacity = m_entities.capacity();
		m_components.reserve(capacity);
		m_entities.reserve(capacity);

		if(oldComponentCapacity != m_components.capacity() ||
			oldEntityCapacity != m_entities.capacity()){
			IncrementStructureVersion();
		}
	}

	std::span<T> GetComponentSpan() noexcept {
		return {m_components.data(), m_components.size()};
	}

	std::span<const T> GetComponentSpan() const noexcept {
		return {m_components.data(), m_components.size()};
	}

	std::span<Entity> GetEntitySpan() noexcept {
		return {m_entities.data(), m_entities.size()};
	}

	std::span<const Entity> GetEntitySpan() const noexcept {
		return {m_entities.data(), m_entities.size()};
	}

	size_t Size() const noexcept {
		return m_components.size();
	}

	uint64_t GetStructureVersion() const noexcept {
		return m_structureVersion;
	}

	std::vector<Entity> GetEntityList() const override {
		return m_entities;
	}

	IComponent* GetEntityComponent(Entity entity) override {
		return Get(entity);
	}

	uint32_t GetComponentGeneration(Entity entity) const override {
		const uint32_t index = entity.GetIndex();
		return index < m_componentGenerations.size()
			? m_componentGenerations[index]
			: 0;
	}

private:
	void EnsureIndexCapacity(uint32_t entityIndex){
		const size_t requiredSize = static_cast<size_t>(entityIndex) + 1;
		if(m_sparseIndices.size() < requiredSize){
			m_sparseIndices.resize(requiredSize, InvalidDenseIndex);
		}
		if(m_componentGenerations.size() < requiredSize){
			m_componentGenerations.resize(requiredSize, 0);
		}
	}

	bool IsDenseIndexValid(size_t denseIndex) const noexcept {
		return denseIndex != InvalidDenseIndex && denseIndex < m_entities.size();
	}

	size_t FindDenseIndex(Entity entity) const noexcept {
		const uint32_t entityIndex = entity.GetIndex();
		if(entityIndex >= m_sparseIndices.size()){
			return InvalidDenseIndex;
		}

		const size_t denseIndex = m_sparseIndices[entityIndex];
		if(!IsDenseIndexValid(denseIndex) || m_entities[denseIndex] != entity){
			return InvalidDenseIndex;
		}
		return denseIndex;
	}

	void RemoveAt(size_t denseIndex){
		const Entity removedEntity = m_entities[denseIndex];
		const size_t lastIndex = m_components.size() - 1;

		if(denseIndex != lastIndex){
			m_components[denseIndex] = std::move(m_components[lastIndex]);
			m_entities[denseIndex] = m_entities[lastIndex];
			m_sparseIndices[m_entities[denseIndex].GetIndex()] = denseIndex;
		}

		m_components.pop_back();
		m_entities.pop_back();
		m_sparseIndices[removedEntity.GetIndex()] = InvalidDenseIndex;
		IncrementGeneration(removedEntity.GetIndex());
		IncrementStructureVersion();
	}

	void IncrementGeneration(uint32_t entityIndex){
		EnsureIndexCapacity(entityIndex);
		uint32_t& generation = m_componentGenerations[entityIndex];
		++generation;
		if(generation == 0){
			++generation;
		}
	}

	void IncrementStructureVersion() noexcept {
		++m_structureVersion;
		if(m_structureVersion == 0){
			++m_structureVersion;
		}
	}

	std::vector<T> m_components;
	std::vector<Entity> m_entities;
	std::vector<size_t> m_sparseIndices;
	std::vector<uint32_t> m_componentGenerations;
	uint64_t m_structureVersion = 0;
};
