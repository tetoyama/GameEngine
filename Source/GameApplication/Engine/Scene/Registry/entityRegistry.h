// =======================================================================
// 
// entityRegistry.h
// 
// =======================================================================
#pragma once

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "Entity/Entity.h"

// Entityの生成・破棄・世代・生存状態を管理するレジストリ。
class EntityRegistry {
public:
	EntityRegistry() = default;
	~EntityRegistry() = default;

	Entity Create() {
		uint32_t index = 0;

		if(!m_recycledIndices.empty()){
			index = m_recycledIndices.back();
			m_recycledIndices.pop_back();
		} else {
			index = m_nextIndex++;
		}

		EnsureCapacity(index);
		m_aliveFlags[index] = true;

		const Entity entity(index, m_generations[index]);
		m_alive.insert(entity);
		return entity;
	}

	// Undo/Redo向けに同じindexを再利用する。
	// 要求された古いgenerationは復活させず、現在のgenerationを使用する。
	Entity CreateID(Entity requested) {
		const uint32_t index = requested.GetIndex();
		if(index == 0) return {};

		EnsureCapacity(index);

		if(m_aliveFlags[index]){
			return Entity(index, m_generations[index]);
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

	// GPU Pickや旧形式の保存データなど、indexしか持たない値から
	// 現在生存しているEntityハンドルを復元する。
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

	// 現在生存しているEntityをすべて破棄する。
	// generationと再利用候補は維持し、古い参照が復活しないようにする。
	void ResetAll() {
		const std::vector<Entity> aliveCopy(m_alive.begin(), m_alive.end());
		for(Entity entity : aliveCopy){
			Destroy(entity);
		}
	}

private:
	void EnsureCapacity(uint32_t index) {
		if(index < m_generations.size()) return;

		const size_t newSize = static_cast<size_t>(index) + 1;
		m_generations.resize(newSize, 0);
		m_aliveFlags.resize(newSize, false);
	}

	void IncrementGeneration(uint32_t index) {
		uint32_t& generation = m_generations[index];
		++generation;

		// 32bit周回時に初期generationへ戻ることを避ける。
		if(generation == 0){
			++generation;
		}
	}

	uint32_t m_nextIndex = 1;
	std::vector<uint32_t> m_recycledIndices;
	std::vector<uint32_t> m_generations{0};
	std::vector<bool> m_aliveFlags{false};
	std::unordered_set<Entity> m_alive;
};
