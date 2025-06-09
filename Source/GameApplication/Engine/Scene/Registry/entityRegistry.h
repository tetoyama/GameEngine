#pragma once

#include <cstdint>
#include <vector>
#include <unordered_set>

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
		} else{
			e = m_nextID++;
		}
		m_alive.insert(e);
		return e;
	}


	void Destroy(Entity e){
		if(m_alive.erase(e) == 0){
			// Œ³‚©‚ç‘¶İ‚µ‚È‚©‚Á‚½i‚Ü‚½‚ÍŠù‚É”jŠüÏ‚İj‚Ì‚Å‰½‚à‚µ‚È‚¢
			return;
		}
		m_recycledIDs.push_back(e);
	}

	bool IsAlive(Entity e) const{
		return m_alive.find(e) != m_alive.end();
	}


	const std::unordered_set<Entity>& GetAllAlive() const{
		return m_alive;
	}

private:
	Entity m_nextID = 1;
	std::vector<Entity> m_recycledIDs;
	std::unordered_set<Entity> m_alive;
};
