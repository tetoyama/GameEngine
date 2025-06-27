#pragma once

#include <cstdint>
#include <unordered_map>
#include <typeindex>
#include <type_traits>
#include <vector>
#include <cassert>
#include <memory>
#include <atomic>
#include <functional>
#include <string>

#include "entityRegistry.h"
#include "Interface/IComponentStorage.h"
#include <yaml-cpp/yaml.h>

// 識別用型
class ComponentType {
public:
	template<typename T>
	static ComponentTypeID Get(){
		static const ComponentTypeID id = s_nextID++;
		return id;
	}
private:
	static std::atomic<ComponentTypeID> s_nextID;
};
class ComponentRegistry {
public:
	using YAMLCreator = std::function<IComponent* (Entity, const YAML::Node&)>;

	explicit ComponentRegistry(EntityRegistry* entityMgr)
		: m_entityManager(entityMgr){}

	// YAML からの生成用ファクトリを登録する
	template<typename T>
	void RegisterYAMLComponent(const std::string& name, bool useArchetype = false){
		if(m_yamlFactory.contains(name)) return;

		RegisterComponent<T>(useArchetype);

		m_yamlFactory[name] = [this](Entity e, const YAML::Node& node) -> IComponent*{
			auto* comp = AddComponent<T>(e);
			if(comp){
				static_cast<T*>(comp)->decode(node);
			}
			return comp;
			};

		m_addComponentFuncs[name] = [this](Entity e){
			AddComponent<T>(e);
			};

		ComponentTypeID typeID = ComponentType::Get<T>();

		m_nameToComponentID[name] = typeID;
		m_componentIDToName[typeID] = name;
		m_typeToID[typeid(T)] = typeID;
		m_idToTypeIndex.emplace(typeID, typeid(T));
		m_ComponentRegistrationOrder.push_back(name);
	}

	IComponent* CreateFromYAML(const std::string& name, Entity e, const YAML::Node& node){
		auto it = m_yamlFactory.find(name);
		if(it != m_yamlFactory.end()){
			return it->second(e, node);
		}
		return nullptr;
	}

	// コンポーネント追加
	template<typename T, typename... Args>
	T* AddComponent(Entity e, Args&&... args){
		assert(m_entityManager->IsAlive(e) && "AddComponent: Entity is not alive");

		std::type_index ti(typeid(T));
		if(!m_storages.contains(ti)){
			RegisterComponent<T>();
		}

		T component(std::forward<Args>(args)...);
		auto& storage = m_storages[ti];
		if(auto* arche = dynamic_cast<ArchetypeStorage<T>*>(storage.get()))
			arche->Add(e, std::move(component));
		else if(auto* sparse = dynamic_cast<SparseStorage<T>*>(storage.get()))
			sparse->Add(e, std::move(component));
		else
			assert(false && "Unknown storage type");

		m_entityMasks[e].set(ComponentType::Get<T>());
		return GetComponent<T>(e);
	}

	// コンポーネント取得
	template<typename T>
	T* GetComponent(Entity e){
		if(!m_entityManager->IsAlive(e)) return nullptr;

		std::type_index ti(typeid(T));
		auto it = m_storages.find(ti);
		if(it == m_storages.end()) return nullptr;

		if(auto* arche = dynamic_cast<ArchetypeStorage<T>*>(it->second.get()))
			return arche->Get(e);
		if(auto* sparse = dynamic_cast<SparseStorage<T>*>(it->second.get()))
			return sparse->Get(e);
		return nullptr;
	}

	// コンポーネント削除
	template<typename T>
	void RemoveComponent(Entity e){
		if(!m_entityManager->IsAlive(e)) return;

		std::type_index ti(typeid(T));
		auto it = m_storages.find(ti);
		if(it != m_storages.end()){
			it->second->Remove(e);
			m_entityMasks[e].reset(ComponentType::Get<T>());
		}
	}

	void RemoveComponentByID(Entity e, ComponentTypeID id){
		auto it = m_idToTypeIndex.find(id);
		if(it != m_idToTypeIndex.end()){
			const auto& ti = it->second;
			auto storageIt = m_storages.find(ti);
			if(storageIt != m_storages.end()){
				storageIt->second->Remove(e);
				m_entityMasks[e].reset(id);
			}
		}
	}

	void OnEntityDestroyed(Entity e){
		for(auto& [_, storage] : m_storages){
			storage->Remove(e);
		}
		m_entityMasks.erase(e);
	}

	// クエリ
	template<typename... Components>
	std::vector<Entity> QueryEntities(){
		std::vector<Entity> result;
		const auto& aliveSet = m_entityManager->GetAllAlive();

		ComponentMask required;
		(required.set(ComponentType::Get<Components>()), ...);

		result.reserve(aliveSet.size());
		for(Entity e : aliveSet){
			auto it = m_entityMasks.find(e);
			if(it != m_entityMasks.end() && (it->second & required) == required){
				result.push_back(e);
			}
		}
		return result;
	}

	template<typename T>
	std::vector<Entity> FindEntitiesWithComponent(){
		std::type_index ti(typeid(T));
		auto it = m_storages.find(ti);
		if(it == m_storages.end()) return {};
		return it->second->GetEntityList();
	}

	std::vector<IComponent*> GetAllComponentsOfEntitySorted(Entity e){
		std::vector<IComponent*> components;

		if(!m_entityManager->IsAlive(e)) return components;

		for(const auto& name : m_ComponentRegistrationOrder){
			auto itID = m_nameToComponentID.find(name);
			if(itID == m_nameToComponentID.end()) continue;

			ComponentTypeID id = itID->second;
			auto itType = m_idToTypeIndex.find(id);
			if(itType == m_idToTypeIndex.end()) continue;

			const std::type_index& ti = itType->second;
			auto storageIt = m_storages.find(ti);
			if(storageIt != m_storages.end()){
				IComponent* comp = storageIt->second->GetEntityComponent(e);
				if(comp){
					components.push_back(comp);
				}
			}
		}
		return components;
	}

	const std::unordered_map<std::string, std::function<void(Entity)>>& GetAddableComponentList() const{
		return m_addComponentFuncs;
	}

	const std::unordered_map<Entity, ComponentMask>& GetEntityMasks() const{
		return m_entityMasks;
	}

	ComponentTypeID GetComponentIDByName(const std::string& name) const{
		auto it = m_nameToComponentID.find(name);
		if(it != m_nameToComponentID.end()){
			return it->second;
		}
		return -1;
	}

	ComponentTypeID GetComponentIDByTypeIndex(const std::type_index& ti) const{
		auto it = m_typeToID.find(ti);
		if(it != m_typeToID.end()){
			return it->second;
		}
		return static_cast<ComponentTypeID>(-1);
	}

	const std::unordered_map<ComponentTypeID, std::string>& GetComponentIDToNameMap() const{
		return m_componentIDToName;
	}

private:
	EntityRegistry* m_entityManager;

	std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> m_storages;
	std::unordered_map<std::type_index, ComponentTypeID> m_typeToID;
	std::unordered_map<ComponentTypeID, std::type_index> m_idToTypeIndex;

	std::unordered_map<Entity, ComponentMask> m_entityMasks;

	std::unordered_map<std::string, YAMLCreator> m_yamlFactory;
	std::unordered_map<std::string, std::function<void(Entity)>> m_addComponentFuncs;
	std::unordered_map<std::string, ComponentTypeID> m_nameToComponentID;
	std::unordered_map<ComponentTypeID, std::string> m_componentIDToName;

	std::vector<std::string> m_ComponentRegistrationOrder;

	// ストレージ登録（内部専用）
	template<typename T>
	void RegisterComponent(bool useArchetype = false){
		std::type_index ti(typeid(T));
		if(m_storages.contains(ti)) return;

		if(useArchetype)
			m_storages[ti] = std::make_unique<ArchetypeStorage<T>>();
		else
			m_storages[ti] = std::make_unique<SparseStorage<T>>();
	}
};
