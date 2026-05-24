// =======================================================================
// 
// IComponentStorage.h
// 
// =======================================================================
#pragma once
#include "Entity/Entity.h"
#include "Interface/IComponent.h"
#include <bitset>

#include <backends/yaml-cpp/yaml.h>

// コンポーネントの型 ID（コンポーネントの種類を一意に識別する）
using ComponentTypeID = uint32_t;

// 最大コンポーネント種類数（ComponentMask のビット数に影響する）
constexpr size_t MAX_COMPONENTS = 64;

// コンポーネントを保有しているかどうかを表すビットセット
// 各ビットが 1 つのコンポーネント種類に対応する
using ComponentMask = std::bitset<MAX_COMPONENTS>;

// コンポーネントストレージの基底インターフェース
// ComponentRegistry がこのインターフェースを通じて各ストレージを操作する
struct IComponentStorage {
	virtual ~IComponentStorage() = default;

	// 指定エンティティのコンポーネントを削除する
	virtual void Remove(Entity e) = 0;

	// 全エンティティのリストを返す
	virtual std::vector<Entity> GetEntityList() const = 0;

	// 指定エンティティのコンポーネントを IComponent* として返す（型消去アクセス）
	virtual IComponent* GetEntityComponent(Entity) = 0;
};

// 配列ベースの高速なコンポーネントストレージ（Archetype 方式）
// エンティティと対応するコンポーネントを平坦な配列で保持し、
// 削除時に末尾要素を移動することで配列の穴を防ぐ（スワップ削除）
// メモリアクセスが連続するため、コンポーネントの一括反復処理に高速
template<typename T>
class ArchetypeStorage: public IComponentStorage {
	static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from IComponent");

public:
	// エンティティにコンポーネントを追加する
	// 既に存在する場合は何もしない
	void Add(Entity e, T component){
		if(m_indexMap.contains(e)) return;
		size_t idx = m_components.size();
		m_indexMap[e] = idx;
		m_components.emplace_back(std::move(component));
		m_entities.emplace_back(e);
	}

	// 指定エンティティのコンポーネントへのポインタを返す
	// 存在しない場合は nullptr を返す
	T* Get(Entity e){
		auto it = m_indexMap.find(e);
		return (it != m_indexMap.end()) ? &m_components[it->second] : nullptr;
	}

	// 指定エンティティのコンポーネントを削除する
	// スワップ削除: 末尾要素を削除位置に移動し、O(1) で削除を実現する
	void Remove(Entity e) override{
		auto it = m_indexMap.find(e);
		if(it == m_indexMap.end()) return;
		size_t idx = it->second;
		size_t last = m_components.size() - 1;

		if(idx != last){
			// 末尾要素を削除対象の位置に上書き移動
			m_components[idx] = std::move(m_components[last]);
			Entity movedEntity = m_entities[last];
			m_entities[idx] = movedEntity;
			m_indexMap[movedEntity] = idx;
		}

		// 末尾を取り除く
		m_components.pop_back();
		m_entities.pop_back();
		m_indexMap.erase(it);
	}

	// 全エンティティのリストを返す（IComponentStorage インターフェースの実装）
	std::vector<Entity> GetEntityList() const override{
		return entities;
	}

	// IComponent* 型でコンポーネントを返す（型消去アクセス）
	IComponent* GetEntityComponent(Entity e) {
		return Get(e);
	}

private:
	std::unordered_map<Entity, size_t> m_IndexMap;  // エンティティ → 配列インデックスのマップ
	std::vector<T> m_Components;                     // コンポーネントの連続配列
	std::vector<Entity> m_Entities;                  // コンポーネントに対応するエンティティ配列
};

// ハッシュマップベースのコンポーネントストレージ（Sparse 方式）
// エンティティ → コンポーネントの unordered_map で管理する
// ランダムアクセスが多いコンポーネントや、エンティティ数が少ない場合に適している
template<typename T>
class SparseStorage: public IComponentStorage {
	static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from IComponent");

public:
	// エンティティにコンポーネントを追加する（既に存在する場合は上書き）
	void Add(Entity e, T component){
		m_map.emplace(e, std::move(component));
	}

	// 指定エンティティのコンポーネントへのポインタを返す
	// 存在しない場合は nullptr を返す
	T* Get(Entity e){
		auto it = m_map.find(e);
		return (it != m_map.end()) ? &it->second : nullptr;
	}

	// 指定エンティティのコンポーネントを削除する
	void Remove(Entity e) override{
		m_map.erase(e);
	}

	// 全エンティティのリストを返す（IComponentStorage インターフェースの実装）
	std::vector<Entity> GetEntityList() const override{
		std::vector<Entity> result;
		result.reserve(m_map.size());
		for(const auto& [entity, _] : m_map)
			result.push_back(entity);
		return result;
	}

	// IComponent* 型でコンポーネントを返す（型消去アクセス）
	IComponent* GetEntityComponent(Entity e) {
		return Get(e);
	}

private:
	std::unordered_map<Entity, T> m_Map;  // エンティティ → コンポーネントのマップ
};
