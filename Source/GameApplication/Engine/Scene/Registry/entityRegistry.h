// =======================================================================
// 
// entityRegistry.h
// 
// =======================================================================
#pragma once

#include <cstdint>
#include <vector>
#include <unordered_set>

#include "Entity/Entity.h"

// エンティティの生成・破棄・生存管理を行うレジストリ
class EntityRegistry {
public:
	EntityRegistry() = default;
	~EntityRegistry() = default;

	Entity Create(){
		Entity e;
		if(!m_recycledIDs.empty()){
			e = m_recycledIDs.back();
			m_recycledIDs.pop_back();
		} else{
			e = m_nextID++;
		}
		m_alive.insert(e);
		return e;
	}

	Entity CreateID(Entity e) {
		if (m_nextID <= e) {
			m_nextID = e + 1;
		}
		m_alive.insert(e);
		return e;
	}

	void Destroy(Entity e){
		if(m_alive.erase(e) == 0){
			// 元から存在しなかった（または既に破棄済み）ので何もしない
			return;
		}
		m_recycledIDs.push_back(e);
	}

	bool IsAlive(Entity e) const{
		return m_alive.find(e) != m_alive.end();
	}

	const std::unordered_set<Entity>& GetAllAlive() const{
		return alive;
	}

	void ResetAll() {
		m_nextID = 1;
		m_recycledIDs.clear();
		m_alive.clear();
	}

private:
	Entity m_NextId= 1;
	std::vector<Entity> m_RecycledIds;
	std::unordered_set<Entity> m_Alive;
};
