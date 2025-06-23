#pragma once

#include <cstdint>
#include <unordered_map>
#include <typeindex>
#include <type_traits>
#include <vector>
#include <cassert>
#include <memory>
#include <atomic>

#include "entityRegistry.h"
#include "Interface/IComponentStorage.h"   // IComponentStorage, ArchetypeStorage<T>, SparseStorage<T> の定義
#include <functional>

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
		// まずはストレージ登録
		RegisterComponent<T>(useArchetype);

		// ファクトリ登録
		m_yamlFactory[name] = [this](Entity e, const YAML::Node& node) -> IComponent*{
			auto* comp = AddComponent<T>(e);
			if(comp){
				// decode() は T に実装されている前提
				static_cast<T*>(comp)->decode(node);
			}
			return comp;
			};
	}

	// YAML 読み込み時に呼び出す汎用 Create
	IComponent* CreateFromYAML(const std::string& name, Entity e, const YAML::Node& node){
		auto it = m_yamlFactory.find(name);
		if(it != m_yamlFactory.end()){
			return it->second(e, node);
		}
		return nullptr;
	}

	// -------------------------------
	// 登録処理
	// -------------------------------
	template<typename T>
	void RegisterComponent(bool useArchetype = false){
		std::type_index ti(typeid(T));
		if(m_storages.contains(ti)) return;

		ComponentTypeID typeID = ComponentType::Get<T>();
		m_typeToID[ti] = typeID;

		if(useArchetype)
			m_storages[ti] = std::make_unique<ArchetypeStorage<T>>();
		else
			m_storages[ti] = std::make_unique<SparseStorage<T>>();
	}

	// -------------------------------
	// Add / Get / Remove
	// -------------------------------
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

	void OnEntityDestroyed(Entity e){
		for(auto& [_, storage] : m_storages){
			storage->Remove(e);
		}
		m_entityMasks.erase(e);
	}

	// -------------------------------
	// クエリ
	// -------------------------------
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

	std::vector<IComponent*> GetAllComponentsOfEntity(Entity e) {
		std::vector<IComponent*> components;

		if (!m_entityManager->IsAlive(e)) {
			return components;
		}

		for (auto& [ti, storage] : m_storages) {
			// すべてのストレージでエンティティに対応するコンポーネントを取得
			IComponent* comp = storage.get()->GetEntityComponent(e);
			
			if (comp) {
				components.push_back(comp);
			}
		}
		return components;
	}
private:
	EntityRegistry* m_entityManager;

	std::unordered_map<Entity, ComponentMask> m_entityMasks;
	std::unordered_map<std::type_index, ComponentTypeID> m_typeToID;
	std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> m_storages;

	// ★ YAML デシリアライズ用ファクトリマップ
	std::unordered_map<std::string, YAMLCreator> m_yamlFactory;
};
