// Engine/Scene/EntityRegistry.h
#pragma once
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <type_traits>
#include <vector>
#include <unordered_map>
#include "Component/IComponent.h"

using EntityID = uint32_t;

class EntityRegistry {
public:
	EntityRegistry();
	~EntityRegistry();

	EntityID CreateEntity();
	void DestroyEntity(EntityID id);

	// Component追加・取得API（テンプレート）
	template<typename T, typename... Args>
	T* AddComponent(EntityID id, Args&&... args){
		static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from IComponent");
		auto comp = std::make_unique<T>(std::forward<Args>(args)...);
		T* ptr = comp.get();
		m_components[std::type_index(typeid(T))][id] = std::move(comp);
		return ptr;
	}

	template<typename T>
	T* GetComponent(EntityID id){
		auto it = m_components.find(std::type_index(typeid(T)));
		if(it != m_components.end()){
			auto entIt = it->second.find(id);
			if(entIt != it->second.end()){
				return static_cast<T*>(entIt->second.get());
			}
		}
		return nullptr;
	}

	// m_components への const アクセスを提供するメソッド
	const auto& GetComponents() const{
		return m_components;
	}

	// m_components への非 const アクセスを提供するメソッド
	auto& GetComponents(){
		return m_components;
	}

	// 特定の型のコンポーネントを持つ全ての Entity を取得
	template<typename T>
	std::vector<EntityID> FindEntitiesWithComponent() const{
		static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from IComponent");
		std::vector<EntityID> result;
		auto it = m_components.find(std::type_index(typeid(T)));
		if(it != m_components.end()){
			for(const auto& [entityID, component] : it->second){
				result.push_back(entityID);
			}
		}
		return result;
	}

private:
	EntityID m_nextEntityID = 1;
	// 型ごと・EntityごとのComponent管理
	std::unordered_map<std::type_index, std::unordered_map<EntityID, std::unique_ptr<IComponent>>> m_components;
};
