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
		if(!m_RecycledIds.empty()){
			e = m_RecycledIds.back();
			m_RecycledIds.pop_back();
		} else{
			e = m_NextId++;
		}
		m_Alive.insert(e);
		return e;
	}

	Entity CreateID(Entity e) {
		if (m_NextId <= e) {
			m_NextId = e + 1;
		}
		m_Alive.insert(e);
		return e;
	}

	void Destroy(Entity e){
		if(m_Alive.erase(e) == 0){
			// 元から存在しなかった（または既に破棄済み）ので何もしない
			return;
		}
		m_RecycledIds.push_back(e);
	}

	bool IsAlive(Entity e) const{
		return m_Alive.find(e) != m_Alive.end();
	}

	const std::unordered_set<Entity>& GetAllAlive() const{
		return m_Alive;
	}

	void ResetAll() {
		m_NextId = 1;
		m_RecycledIds.clear();
		m_Alive.clear();
	}

private:
	Entity m_NextId= 1;
	std::vector<Entity> m_RecycledIds;
	std::unordered_set<Entity> m_Alive;
};
