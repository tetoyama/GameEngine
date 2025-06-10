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

/**
 * @brief グローバルに一意な ComponentTypeID を生成するユーティリティ
 *
 * static 変数をテンプレートごとに持つことで、
 * 複数の ComponentRegistry インスタンスをまたいでも一意性が保証される。
 */
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



/**
 * @brief Component の登録・追加・取得・削除、および Entity に紐づくビットマスクを管理するクラス
 *
 * 内部では typeid(T) をキーとした IComponentStorage の固有インスタンスを持ち、
 * ArchetypeStorage<T> や SparseStorage<T> を動的に切り替えて保持可能。
 * また、Entityごとの ComponentMask を持つことで QueryEntities(Ts...) が可能。
 */
class ComponentRegistry {
public:
	ComponentRegistry(EntityRegistry* entityMgr)
		: m_entityManager(entityMgr){}

	~ComponentRegistry() = default;

	// -----------------------------------
	// Component 登録
	// -----------------------------------
	template<typename T>
	void RegisterComponent(bool useArchetype = false){
		std::type_index ti(typeid(T));
		if(m_storages.find(ti) != m_storages.end()){
			// 既に登録済み
			return;
		}

		// 一意な ComponentTypeID を取得してメンバに保持
		ComponentTypeID typeID = ComponentType::Get<T>();
		m_typeToID[ti] = typeID;

		// 実際のストレージを生成
		if(useArchetype){
			m_storages[ti] = std::make_unique<ArchetypeStorage<T>>();
		} else{
			m_storages[ti] = std::make_unique<SparseStorage<T>>();
		}
	}

	// -----------------------------------
	// Component 追加 / 取得 / 削除
	// -----------------------------------
	template<typename T, typename... Args>
	T* AddComponent(Entity e, Args&&... args){
		assert(m_entityManager->IsAlive(e) && "AddComponent: 無効な Entity を指定しています");

		std::type_index ti(typeid(T));
		auto it = m_storages.find(ti);
		if(it == m_storages.end()){
			// 未登録ならデフォルトで SparseStorage を使って登録
			RegisterComponent<T>(false);
			it = m_storages.find(ti);
		}

		// コンポーネントインスタンスを追加
		T component(std::forward<Args>(args)...);
		if(auto arche = dynamic_cast<ArchetypeStorage<T>*>(it->second.get())){
			arche->Add(e, component);
		} else if(auto sparse = dynamic_cast<SparseStorage<T>*>(it->second.get())){
			sparse->Add(e, component);
		} else{
			assert(false && "AddComponent: 想定外のストレージタイプです");
		}

		// Entity のビットマスクに対応ビットを立てる
		ComponentTypeID typeID = ComponentType::Get<T>();
		m_entityMasks[e].set(typeID);
		return GetComponent<T>(e);
	}

	template<typename T>
	T* GetComponent(Entity e){
		if(!m_entityManager->IsAlive(e)) return nullptr;

		std::type_index ti(typeid(T));
		auto it = m_storages.find(ti);
		if(it == m_storages.end()) return nullptr;

		if(auto arche = dynamic_cast<ArchetypeStorage<T>*>(it->second.get())){
			return arche->Get(e);
		} else if(auto sparse = dynamic_cast<SparseStorage<T>*>(it->second.get())){
			return sparse->Get(e);
		}
		return nullptr;
	}

	template<typename T>
	void RemoveComponent(Entity e){
		if(!m_entityManager->IsAlive(e)) return;

		std::type_index ti(typeid(T));
		auto it = m_storages.find(ti);
		if(it == m_storages.end()) return;

		it->second->Remove(e);
		ComponentTypeID typeID = ComponentType::Get<T>();
		m_entityMasks[e].reset(typeID);
	}

	// -----------------------------------
	// Entity 破棄時に呼ぶ（全コンポーネントを削除してマスクもクリア）
	// -----------------------------------
	void OnEntityDestroyed(Entity e){
		// 生存チェックは EntityManager が行っている前提
		// 各ストレージからコンポーネントを一括で除去
		for(auto& kv : m_storages){
			kv.second->Remove(e);
		}
		// マスク情報を消去
		m_entityMasks.erase(e);
	}

	// -----------------------------------
	// クエリ：複数コンポーネントを持つ Entity を列挙する
	// -----------------------------------
	template<typename... Components>
	std::vector<Entity> QueryEntities(){
		std::vector<Entity> result;
		const auto& aliveSet = m_entityManager->GetAllAlive();

		// 必要なマスクを作成
		ComponentMask required;
		(void)std::initializer_list<int>{ (required.set(ComponentType::Get<Components>()), 0)... };

		result.reserve(aliveSet.size());
		for(Entity e : aliveSet){
			const auto itMask = m_entityMasks.find(e);
			if(itMask == m_entityMasks.end()) continue;

			const ComponentMask& mask = itMask->second;
			if((mask & required) == required){
				result.push_back(e);
			}
		}
		return result;
	}

	// -----------------------------------
	// 型 T が登録されている Storage に登録されている全 Entity を取得
	// （Archetype/Sparse 共通）
	// -----------------------------------
	template<typename T>
	std::vector<Entity> FindEntitiesWithComponent(){
		std::type_index ti(typeid(T));
		auto it = m_storages.find(ti);
		if(it == m_storages.end()){
			return {};
		}

		// IComponentStorage の実装側で GetEntityList() を提供している想定
		return it->second->GetEntityList();
	}

private:
	EntityRegistry* m_entityManager;

	// Entity → ComponentMask
	std::unordered_map<Entity, ComponentMask> m_entityMasks;

	// 型情報 → ComponentTypeID
	std::unordered_map<std::type_index, ComponentTypeID> m_typeToID;

	// 型情報 → ストレージ本体（ArchetypeStorage<T> または SparseStorage<T>）
	std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> m_storages;
};
