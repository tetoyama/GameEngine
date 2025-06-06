#pragma once
#include "Entity.h"
#include <bitset>

using ComponentTypeID = uint32_t;
constexpr size_t MAX_COMPONENTS = 64;  // 必要に応じて調整
using ComponentMask = std::bitset<MAX_COMPONENTS>;

// すべてのストレージが継承する抽象基底クラス
struct IComponentStorage {
    virtual ~IComponentStorage() = default;
    virtual void Remove(Entity e) = 0;
};

template<typename T>
class ArchetypeStorage : public IComponentStorage {
    static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from IComponent");
public:
    std::unordered_map<Entity, size_t> m_indexMap;
    std::vector<T> m_components;
    std::vector<Entity> m_entities;

    void Add(Entity e, T component) {
        if (m_indexMap.find(e) != m_indexMap.end()) return; // 既存なら無視（上書き不可）
        size_t idx = m_components.size();
        m_indexMap[e] = idx;
        m_components.push_back(std::move(component));
        m_entities.push_back(e);
    }

    T* Get(Entity e) {
        auto it = m_indexMap.find(e);
        if (it == m_indexMap.end()) return nullptr;
        return &m_components[it->second];
    }

    void Remove(Entity e) override {
        auto it = m_indexMap.find(e);
        if (it == m_indexMap.end()) return;
        size_t idx = it->second;
        size_t last = m_components.size() - 1;
        if (idx != last) {
            // 最後の要素を削除箇所にスワップ
            m_components[idx] = std::move(m_components[last]);
            Entity movedEntity = m_entities[last];
            m_entities[idx] = movedEntity;
            m_indexMap[movedEntity] = idx;
        }
        m_components.pop_back();
        m_entities.pop_back();
        m_indexMap.erase(it);
    }

    const std::vector<Entity>& GetEntityList() const {
        return m_entities;
    }
};

template<typename T>
class SparseStorage : public IComponentStorage {
    static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from IComponent");
public:
    std::unordered_map<Entity, T> m_map;

    void Add(Entity e, T component) {
        m_map.emplace(e, std::move(component));
    }

    T* Get(Entity e) {
        auto it = m_map.find(e);
        if (it == m_map.end()) return nullptr;
        return &it->second;
    }

    void Remove(Entity e) override {
        m_map.erase(e);
    }

    std::vector<Entity> GetEntityList() const {
        std::vector<Entity> result;
        result.reserve(m_map.size());
        for (auto& kv : m_map) {
            result.push_back(kv.first);
        }
        return result;
    }
};

