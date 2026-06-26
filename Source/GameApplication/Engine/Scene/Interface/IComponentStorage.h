// =======================================================================
//
// IComponentStorage.h
//
// =======================================================================
#pragma once

#include "Entity/Entity.h"

#include <algorithm>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

using ComponentTypeID = uint32_t;
constexpr ComponentTypeID INVALID_COMPONENT_TYPE_ID =
	(std::numeric_limits<ComponentTypeID>::max)();
constexpr size_t MAX_COMPONENTS = 64;
using ComponentMask = std::bitset<MAX_COMPONENTS>;

struct ComponentView {
	ComponentTypeID typeID = INVALID_COMPONENT_TYPE_ID;
	void* data = nullptr;
	constexpr explicit operator bool() const noexcept {
		return typeID != INVALID_COMPONENT_TYPE_ID && data != nullptr;
	}
	template<typename T>
	T* As() const noexcept { return static_cast<T*>(data); }
};

struct ConstComponentView {
	ComponentTypeID typeID = INVALID_COMPONENT_TYPE_ID;
	const void* data = nullptr;
	constexpr explicit operator bool() const noexcept {
		return typeID != INVALID_COMPONENT_TYPE_ID && data != nullptr;
	}
	template<typename T>
	const T* As() const noexcept { return static_cast<const T*>(data); }
};

struct ComponentStorageTelemetry {
	size_t currentSize = 0;
	size_t peakSize = 0;
	size_t capacity = 0;
	size_t growthEventCount = 0;
};

struct IComponentStorage {
	virtual ~IComponentStorage() = default;
	virtual void Remove(Entity entity) = 0;
	virtual bool Contains(Entity entity) const = 0;
	virtual void* GetRaw(Entity entity) = 0;
	virtual const void* GetRaw(Entity entity) const = 0;
	virtual std::vector<Entity> GetEntityList() const = 0;
	virtual size_t Size() const noexcept = 0;

	virtual void Reserve(size_t expectedCount){ (void)expectedCount; }
	virtual void PreallocatePages(size_t pageCount){ (void)pageCount; }
	virtual size_t Capacity() const noexcept { return Size(); }

	// Runtime利用量と実データ容量拡張をStorage方式共通で公開する。
	// Reserve / PreallocatePagesによる初期確保はGrowth Eventへ含めない。
	virtual size_t PeakSize() const noexcept { return Size(); }
	virtual size_t GrowthEventCount() const noexcept { return 0; }
	virtual void ResetPeakMetrics() noexcept {}

	ComponentStorageTelemetry Telemetry() const noexcept {
		return ComponentStorageTelemetry{
			Size(),
			PeakSize(),
			Capacity(),
			GrowthEventCount()
		};
	}

	virtual uint32_t GetComponentGeneration(Entity entity) const = 0;
	virtual uint64_t GetStructureVersion() const noexcept = 0;
};

template<typename T>
class DenseComponentPool final: public IComponentStorage {
public:
	static constexpr uint32_t InvalidDenseIndex =
		(std::numeric_limits<uint32_t>::max)();

	void Add(Entity entity, T component){
		EnsureIndexCapacity(entity.GetIndex());
		if(Contains(entity)) return;

		const uint32_t existingIndex = m_sparseIndices[entity.GetIndex()];
		if(existingIndex != InvalidDenseIndex && existingIndex < m_entities.size()){
			Remove(m_entities[existingIndex]);
		}

		const size_t capacityBefore = m_components.capacity();
		const uint32_t denseIndex = static_cast<uint32_t>(m_components.size());
		m_components.emplace_back(std::move(component));
		m_entities.emplace_back(entity);
		m_sparseIndices[entity.GetIndex()] = denseIndex;
		if(m_components.capacity() > capacityBefore) ++m_growthEventCount;
		m_peakSize = (std::max)(m_peakSize, m_components.size());
		++m_structureVersion;
	}

	T* Get(Entity entity){
		const uint32_t denseIndex = FindDenseIndex(entity);
		return denseIndex != InvalidDenseIndex ? &m_components[denseIndex] : nullptr;
	}
	const T* Get(Entity entity) const {
		const uint32_t denseIndex = FindDenseIndex(entity);
		return denseIndex != InvalidDenseIndex ? &m_components[denseIndex] : nullptr;
	}
	bool Contains(Entity entity) const override {
		return FindDenseIndex(entity) != InvalidDenseIndex;
	}

	void Remove(Entity entity) override {
		const uint32_t denseIndex = FindDenseIndex(entity);
		if(denseIndex == InvalidDenseIndex) return;

		const uint32_t lastIndex = static_cast<uint32_t>(m_components.size() - 1);
		if(denseIndex != lastIndex){
			m_components[denseIndex] = std::move(m_components[lastIndex]);
			m_entities[denseIndex] = m_entities[lastIndex];
			m_sparseIndices[m_entities[denseIndex].GetIndex()] = denseIndex;
		}
		m_components.pop_back();
		m_entities.pop_back();
		m_sparseIndices[entity.GetIndex()] = InvalidDenseIndex;
		IncrementGeneration(entity.GetIndex());
		++m_structureVersion;
	}

	void* GetRaw(Entity entity) override { return Get(entity); }
	const void* GetRaw(Entity entity) const override { return Get(entity); }
	std::vector<Entity> GetEntityList() const override { return m_entities; }
	size_t Size() const noexcept override { return m_components.size(); }

	void Reserve(size_t expectedCount) override {
		m_components.reserve(expectedCount);
		m_entities.reserve(expectedCount);
		m_sparseIndices.reserve(expectedCount);
		m_componentGenerations.reserve(expectedCount);
	}
	size_t Capacity() const noexcept override { return m_components.capacity(); }
	size_t PeakSize() const noexcept override { return m_peakSize; }
	size_t GrowthEventCount() const noexcept override { return m_growthEventCount; }
	void ResetPeakMetrics() noexcept override {
		m_peakSize = m_components.size();
		m_growthEventCount = 0;
	}

	uint32_t GetComponentGeneration(Entity entity) const override {
		const uint32_t index = entity.GetIndex();
		return index < m_componentGenerations.size()
			? m_componentGenerations[index]
			: 0;
	}
	uint64_t GetStructureVersion() const noexcept override {
		return m_structureVersion;
	}

	std::span<T> Components() noexcept { return m_components; }
	std::span<const T> Components() const noexcept { return m_components; }
	std::span<Entity> Entities() noexcept { return m_entities; }
	std::span<const Entity> Entities() const noexcept { return m_entities; }

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

	uint32_t FindDenseIndex(Entity entity) const {
		const uint32_t entityIndex = entity.GetIndex();
		if(entityIndex >= m_sparseIndices.size()) return InvalidDenseIndex;
		const uint32_t denseIndex = m_sparseIndices[entityIndex];
		if(denseIndex == InvalidDenseIndex || denseIndex >= m_entities.size()){
			return InvalidDenseIndex;
		}
		return m_entities[denseIndex] == entity ? denseIndex : InvalidDenseIndex;
	}

	void IncrementGeneration(uint32_t entityIndex){
		EnsureIndexCapacity(entityIndex);
		uint32_t& generation = m_componentGenerations[entityIndex];
		++generation;
		if(generation == 0) ++generation;
	}

	std::vector<T> m_components;
	std::vector<Entity> m_entities;
	std::vector<uint32_t> m_sparseIndices;
	std::vector<uint32_t> m_componentGenerations;
	size_t m_peakSize = 0;
	size_t m_growthEventCount = 0;
	uint64_t m_structureVersion = 0;
};

template<typename T>
class SparseStorage final: public IComponentStorage {
public:
	void Add(Entity entity, T component){
		if(m_components.contains(entity)) return;
		const size_t bucketCountBefore = m_components.bucket_count();
		m_components.emplace(entity, std::move(component));
		m_componentGenerations.try_emplace(entity.GetIndex(), 0);
		if(m_components.bucket_count() > bucketCountBefore) ++m_growthEventCount;
		m_peakSize = (std::max)(m_peakSize, m_components.size());
		++m_structureVersion;
	}

	T* Get(Entity entity){
		auto iterator = m_components.find(entity);
		return iterator != m_components.end() ? &iterator->second : nullptr;
	}
	const T* Get(Entity entity) const {
		auto iterator = m_components.find(entity);
		return iterator != m_components.end() ? &iterator->second : nullptr;
	}
	bool Contains(Entity entity) const override { return m_components.contains(entity); }

	void Remove(Entity entity) override {
		auto iterator = m_components.find(entity);
		if(iterator == m_components.end()) return;
		m_components.erase(iterator);
		IncrementGeneration(entity.GetIndex());
		++m_structureVersion;
	}

	void* GetRaw(Entity entity) override { return Get(entity); }
	const void* GetRaw(Entity entity) const override { return Get(entity); }
	std::vector<Entity> GetEntityList() const override {
		std::vector<Entity> entities;
		entities.reserve(m_components.size());
		for(const auto& [entity, component] : m_components){
			(void)component;
			entities.push_back(entity);
		}
		return entities;
	}
	size_t Size() const noexcept override { return m_components.size(); }
	void Reserve(size_t expectedCount) override {
		m_components.reserve(expectedCount);
		m_componentGenerations.reserve(expectedCount);
	}
	size_t Capacity() const noexcept override { return m_components.bucket_count(); }
	size_t PeakSize() const noexcept override { return m_peakSize; }
	size_t GrowthEventCount() const noexcept override { return m_growthEventCount; }
	void ResetPeakMetrics() noexcept override {
		m_peakSize = m_components.size();
		m_growthEventCount = 0;
	}

	uint32_t GetComponentGeneration(Entity entity) const override {
		auto iterator = m_componentGenerations.find(entity.GetIndex());
		return iterator != m_componentGenerations.end() ? iterator->second : 0;
	}
	uint64_t GetStructureVersion() const noexcept override {
		return m_structureVersion;
	}

private:
	void IncrementGeneration(uint32_t entityIndex){
		uint32_t& generation = m_componentGenerations[entityIndex];
		++generation;
		if(generation == 0) ++generation;
	}

	std::unordered_map<Entity, T> m_components;
	std::unordered_map<uint32_t, uint32_t> m_componentGenerations;
	size_t m_peakSize = 0;
	size_t m_growthEventCount = 0;
	uint64_t m_structureVersion = 0;
};

template<typename T>
using ArchetypeStorage = DenseComponentPool<T>;
