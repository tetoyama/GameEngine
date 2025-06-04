// Engine/Scene/entityRegistry.cpp
#include "entityRegistry.h"
#include <unordered_map>
#include <algorithm>
#include <vector>

EntityRegistry::EntityRegistry()
	: m_nextEntityID(1){}

EntityRegistry::~EntityRegistry(){
	std::vector<Entity> allEntities;

	// まず全EntityIDを収集（重複排除のためsetでも可）
	for(auto compMapIt = m_components.begin(); compMapIt != m_components.end(); ++compMapIt){
		const auto& entityMap = compMapIt->second;

		for(auto entityIt = entityMap.begin(); entityIt != entityMap.end(); ++entityIt){
			Entity id = entityIt->first;
			allEntities.push_back(id);
		}
	}

	// 重複エンティティIDを削除（同一Entityが複数Componentを持つ可能性があるため）
	std::sort(allEntities.begin(), allEntities.end());
	allEntities.erase(std::unique(allEntities.begin(), allEntities.end()), allEntities.end());

	// 各Entityを削除
	for(Entity id : allEntities){
		DestroyEntity(id);
	}
}

Entity EntityRegistry::CreateEntity(){
	Entity id = m_nextEntityID++;
	return id;
}

void EntityRegistry::DestroyEntity(Entity id){
	for(auto& compMap : m_components){
		compMap.second.erase(id);
	}
}

