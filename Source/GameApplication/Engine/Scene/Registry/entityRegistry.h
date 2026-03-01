#pragma once

#include <cstdint>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "Entity/Entity.h"

class EntityRegistry {
public:
	EntityRegistry() = default;
	~EntityRegistry() = default;

	Entity Create(){
		Entity e;
		if(!m_recycledIDs.empty()){
			e = m_recycledIDs.back();
			m_recycledIDs.pop_back();
			// 世代はすでに Destroy 時にインクリメント済み
		} else{
			e = m_nextID++;
			m_generations[e] = 0; // 新規 ID の世代は 0 から始まる
		}
		m_alive.insert(e);
		return e;
	}

	Entity CreateID(Entity e) {
		if (m_nextID <= e) {
			m_nextID = e + 1;
		}
		if (m_generations.find(e) == m_generations.end()) {
			m_generations[e] = 0;
		}
		m_alive.insert(e);
		return e;
	}

	void Destroy(Entity e){
		if(m_alive.erase(e) == 0){
			// 元から存在しなかった（または既に破棄済み）ので何もしない
			return;
		}
		// ID を再利用する前に世代をインクリメントして古いリファレンスを無効化する
		m_generations[e]++;
		m_recycledIDs.push_back(e);
	}

	bool IsAlive(Entity e) const{
		return m_alive.find(e) != m_alive.end();
	}

	// 世代つきの生存確認。EntityRef / ComponentRef の IsValid() で使用する
	bool IsAliveWithGeneration(Entity e, uint32_t generation) const {
		auto it = m_generations.find(e);
		if (it == m_generations.end()) return false;
		if (it->second != generation) return false;
		return m_alive.find(e) != m_alive.end();
	}

	// エンティティの現在の世代を返す。存在しない場合は 0 を返す
	uint32_t GetGeneration(Entity e) const {
		auto it = m_generations.find(e);
		return (it != m_generations.end()) ? it->second : 0;
	}

	const std::unordered_set<Entity>& GetAllAlive() const{
		return m_alive;
	}

	void ResetAll() {
		m_nextID = 1;
		m_recycledIDs.clear();
		m_alive.clear();
		m_generations.clear();
	}

private:
	Entity m_nextID = 1;
	std::vector<Entity> m_recycledIDs;
	std::unordered_set<Entity> m_alive;
	std::unordered_map<Entity, uint32_t> m_generations; // エンティティごとの世代カウンタ
};
