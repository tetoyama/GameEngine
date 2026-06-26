// =======================================================================
// 
// entityRegistry.h
// 
// =======================================================================
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "Entity/Entity.h"
#include "Config/SceneStorageConfig.h"

class EntityRegistry {
public:
	EntityRegistry(){
		Reserve(SceneStorageConfig::DefaultExpectedEntityCount);
	}
	~EntityRegistry() = default;

	void Reserve(size_t expectedEntityCount){
		const size_t slotCount = expectedEntityCount + 1;
		m_generations.reserve(slotCount);
		m_aliveFlags.reserve(slotCount);
		m_recycledIndices.reserve(expectedEntityCount);
		m_alive.reserve(expectedEntityCount);
	}

	void SetRuntimeGrowthAllowed(bool allowed) noexcept {
		m_allowRuntimeGrowth = allowed;
	}

	bool IsRuntimeGrowthAllowed() const noexcept {
		return m_allowRuntimeGrowth;
	}

	Entity Create() {
		const bool reuseRecycledIndex = !m_recycledIndices.empty();
		const uint32_t index = reuseRecycledIndex
			? m_recycledIndices.back()
			: m_nextIndex;

		if(!CanInsertAliveEntity() || !EnsureCapacity(index)){
			return {};
		}

		if(reuseRecycledIndex){
			m_recycledIndices.pop_back();
		}else{
			++m_nextIndex;
		}

		m_aliveFlags[index] = true;
		const Entity entity(index, m_generations[index]);
		m_alive.insert(entity);
		m_peakAliveCount = (std::max)(m_peakAliveCount, m_alive.size());
		return entity;
	}

	// Undo/Redo向けに同じindexを再利用する。
	// 要求された古いgenerationは復活させず、現在のgenerationを使用する。
	Entity CreateID(Entity requested) {
		const uint32_t index = requested.GetIndex();
		if(index == 0) return {};

		if(index < m_aliveFlags.size() && m_aliveFlags[index]){
			return Entity(index, m_generations[index]);
		}

		if(!CanInsertAliveEntity() || !EnsureCapacity(index)){
			return {};
		}

		if(m_nextIndex <= index){
			m_nextIndex = index + 1;
		}

		auto recycledIt = std::find(
			m_recycledIndices.begin(),
			m_recycledIndices.end(),
			index
		);
		if(recycledIt != m_recycledIndices.end()){
			m_recycledIndices.erase(recycledIt);
		}

		m_aliveFlags[index] = true;
		const Entity entity(index, m_generations[index]);
		m_alive.insert(entity);
		m_peakAliveCount = (std::max)(m_peakAliveCount, m_alive.size());
		return entity;
	}

	void Destroy(Entity entity) {
		if(!IsAlive(entity)){
			return;
		}

		m_alive.erase(entity);
		m_aliveFlags[entity.GetIndex()] = false;
		IncrementGeneration(entity.GetIndex());
		m_recycledIndices.push_back(entity.GetIndex());
	}

	bool IsAlive(Entity entity) const {
		const uint32_t index = entity.GetIndex();
		if(index == 0 || index >= m_aliveFlags.size()){
			return false;
		}

		return m_aliveFlags[index] &&
			m_generations[index] == entity.GetGeneration();
	}

	Entity Resolve(uint32_t index) const {
		if(index == 0 || index >= m_aliveFlags.size() || !m_aliveFlags[index]){
			return {};
		}
		return Entity(index, m_generations[index]);
	}

	uint32_t GetGeneration(uint32_t index) const {
		return index < m_generations.size() ? m_generations[index] : 0;
	}

	const std::unordered_set<Entity>& GetAllAlive() const {
		return m_alive;
	}

	size_t GetAliveCount() const noexcept {
		return m_alive.size();
	}

	size_t GetPeakAliveCount() const noexcept {
		return m_peakAliveCount;
	}

	size_t GetCapacity() const noexcept {
		const size_t slotCapacity = (std::min)(
			m_generations.capacity(),
			m_aliveFlags.capacity()
		);
		return slotCapacity > 0 ? slotCapacity - 1 : 0;
	}

	size_t GetGrowthEventCount() const noexcept {
		return m_growthEventCount;
	}

	void ResetPeakMetrics() noexcept {
		m_peakAliveCount = m_alive.size();
		m_growthEventCount = 0;
	}

	void ResetAll() {
		const std::vector<Entity> aliveCopy(m_alive.begin(), m_alive.end());
		for(Entity entity : aliveCopy){
			Destroy(entity);
		}
	}

private:
	bool EnsureCapacity(uint32_t index) {
		if(index < m_generations.size()) return true;

		const size_t newSize = static_cast<size_t>(index) + 1;
		if(!m_allowRuntimeGrowth &&
			(newSize > m_generations.capacity() ||
			 newSize > m_aliveFlags.capacity())){
			return false;
		}

		const size_t previousGenerationCapacity = m_generations.capacity();
		const size_t previousAliveFlagCapacity = m_aliveFlags.capacity();
		m_generations.resize(newSize, 0);
		m_aliveFlags.resize(newSize, false);
		if(m_generations.capacity() > previousGenerationCapacity ||
			m_aliveFlags.capacity() > previousAliveFlagCapacity){
			++m_growthEventCount;
		}
		return true;
	}

	bool CanInsertAliveEntity() const noexcept {
		if(m_allowRuntimeGrowth) return true;
		const float nextSize = static_cast<float>(m_alive.size() + 1);
		const float threshold =
			static_cast<float>(m_alive.bucket_count()) *
			m_alive.max_load_factor();
		return nextSize <= threshold;
	}

	void IncrementGeneration(uint32_t index) {
		uint32_t& generation = m_generations[index];
		++generation;
		if(generation == 0){
			++generation;
		}
	}

	uint32_t m_nextIndex = 1;
	std::vector<uint32_t> m_recycledIndices;
	std::vector<uint32_t> m_generations{0};
	std::vector<bool> m_aliveFlags{false};
	std::unordered_set<Entity> m_alive;
	size_t m_peakAliveCount = 0;
	size_t m_growthEventCount = 0;
	bool m_allowRuntimeGrowth = true;
};
