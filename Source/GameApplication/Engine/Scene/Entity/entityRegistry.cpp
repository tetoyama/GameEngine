// Engine/Scene/entityRegistry.cpp
#include "entityRegistry.h"
#include <unordered_map>
#include <algorithm>
#include <vector>

EntityRegistry::EntityRegistry()
	: m_nextEntityID(1){}

EntityRegistry::~EntityRegistry(){
	std::vector<EntityID> allEntities;

	// まず全EntityIDを収集（重複排除のためsetでも可）
	for(auto compMapIt = m_components.begin(); compMapIt != m_components.end(); ++compMapIt){
		const auto& entityMap = compMapIt->second;

		for(auto entityIt = entityMap.begin(); entityIt != entityMap.end(); ++entityIt){
			EntityID id = entityIt->first;
			allEntities.push_back(id);
		}
	}

	// 重複エンティティIDを削除（同一Entityが複数Componentを持つ可能性があるため）
	std::sort(allEntities.begin(), allEntities.end());
	allEntities.erase(std::unique(allEntities.begin(), allEntities.end()), allEntities.end());

	// 各Entityを削除
	for(EntityID id : allEntities){
		DestroyEntity(id);
	}
}

EntityID EntityRegistry::CreateEntity(){
	EntityID id = m_nextEntityID++;
	return id;
}

void EntityRegistry::DestroyEntity(EntityID id){
	for(auto& compMap : m_components){
		compMap.second.erase(id);
	}
}

