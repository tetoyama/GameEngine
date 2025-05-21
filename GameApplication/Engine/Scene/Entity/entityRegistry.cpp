// Engine/Scene/entityRegistry.cpp
#include "entityRegistry.h"
#include <unordered_map>

EntityRegistry::EntityRegistry()
	: m_nextEntityID(1){}

EntityRegistry::~EntityRegistry(){
	// 必要ならリソース解放処理
}

EntityID EntityRegistry::CreateEntity(){
	EntityID id = m_nextEntityID++;
	// 必要ならエンティティの初期化処理
	return id;
}

void EntityRegistry::DestroyEntity(EntityID id){
	// 各Componentストレージから該当EntityのComponentを削除
	for(auto& compMap : m_components){
		compMap.second.erase(id);
	}
	// 必要ならエンティティ自体の管理情報も削除
}

