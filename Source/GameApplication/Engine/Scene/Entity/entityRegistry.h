// Engine/Scene/EntityRegistry.h
#pragma once
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <type_traits>
#include <vector>
#include <assert.h>
#include "Interface/IComponent.h"
#include "Interface/Entity.h"
#include"Interface/ISystem.h"
#include "Interface/IComponentStorage.h"
#include <unordered_set>

class EntityRegistry {
public:
    EntityRegistry() = default;
    ~EntityRegistry() = default;

    // -----------------------------------
    // Entity 作成 / 破棄
    // -----------------------------------
    Entity CreateEntity() {
        Entity e;
        if (!m_recycledIDs.empty()) {
            // 再利用可能IDがあれば流用
            e = m_recycledIDs.back();
            m_recycledIDs.pop_back();
        } else {
            e = m_nextID++;
        }
        m_alive.insert(e);
        m_entityMasks[e].reset();  // 新規作成時は全ビットクリア
        return e;
    }
    void DestroyEntity(Entity e) {
        if (m_alive.erase(e) == 0) return;
        // 各ストレージからコンポーネントを削除
        for (auto& kv : m_storages) {
            kv.second->Remove(e);
        }
        // マスク情報をクリア
        m_entityMasks.erase(e);
        // IDを再利用リストに追加
        m_recycledIDs.push_back(e);
    }

    // -----------------------------------
    // コンポーネント登録
    // -----------------------------------
    template<typename T>
    void RegisterComponent(bool useArchetype) {
        std::type_index ti = std::type_index(typeid(T));
        if (m_storages.find(ti) != m_storages.end()) {
            return; // 既に登録済み
        }

        // 新しい ComponentTypeID を割り当て
        ComponentTypeID id = GetComponentTypeID<T>();
        m_typeToID[ti] = id;

        // ストレージを生成
        if (useArchetype) {
            m_storages[ti] = std::make_unique<ArchetypeStorage<T>>();
        } else {
            m_storages[ti] = std::make_unique<SparseStorage<T>>();
        }
    }

    // -----------------------------------
    // コンポーネント追加 / 取得
    // -----------------------------------
    template<typename T, typename... Args>
    T* AddComponent(Entity e, Args&&... args) {
        std::type_index ti = std::type_index(typeid(T));
        auto it = m_storages.find(ti);
        if (it == m_storages.end()) {
            // 未登録ならデフォルトは SparseStorage
            RegisterComponent<T>(false);
            it = m_storages.find(ti);
        }

        // ストレージの型を判定して追加
        if (auto arche = dynamic_cast<ArchetypeStorage<T>*>(it->second.get())) {
            arche->Add(e, T{ std::forward<Args>(args)... });
        } else if (auto sparse = dynamic_cast<SparseStorage<T>*>(it->second.get())) {
            sparse->Add(e, T{ std::forward<Args>(args)... });
        } else {
            assert(false && "Unknown storage type");
            return nullptr;
        }

        // マスクに対応ビットをセット
        m_entityMasks[e].set(GetComponentTypeID<T>());
        return GetComponent<T>(e);
    }

    template<typename T>
    T* GetComponent(Entity e) {
        std::type_index ti = std::type_index(typeid(T));
        auto it = m_storages.find(ti);
        if (it == m_storages.end()) return nullptr;

        if (auto arche = dynamic_cast<ArchetypeStorage<T>*>(it->second.get())) {
            return arche->Get(e);
        } else if (auto sparse = dynamic_cast<SparseStorage<T>*>(it->second.get())) {
            return sparse->Get(e);
        }
        return nullptr;
    }

    template<typename T>
    void RemoveComponent(Entity e) {
        std::type_index ti = std::type_index(typeid(T));
        auto it = m_storages.find(ti);
        if (it == m_storages.end()) return;

        it->second->Remove(e);
        m_entityMasks[e].reset(GetComponentTypeID<T>());
    }

    template<typename... Components>
    std::vector<Entity> QueryEntities() {
        // 必要なビットマスクを作成
        ComponentMask requiredMask;

        // 展開でそれぞれの ComponentTypeID に対してビットを立てる
        int dummy[] = { (requiredMask.set(GetComponentTypeID<Components>()), 0)... };
        (void)dummy; // unused変数対策

        std::vector<Entity> result;
        result.reserve(m_alive.size());
        for (auto e : m_alive) {
            const ComponentMask& mask = m_entityMasks[e];
            // mask が requiredMask をすべて包含していれば含める
            if ((mask & requiredMask) == requiredMask) {
                result.push_back(e);
            }
        }
        return result;
    }

    // Archetype / Sparse を問わず Entity 一覧を取得
    template<typename T>
    std::vector<Entity> FindEntitiesWithComponent() {
        std::type_index ti = std::type_index(typeid(T));
        auto it = m_storages.find(ti);
        if (it == m_storages.end()) return {};

        if (auto arche = dynamic_cast<ArchetypeStorage<T>*>(it->second.get())) {
            return arche->GetEntityList();
        } else if (auto sparse = dynamic_cast<SparseStorage<T>*>(it->second.get())) {
            return sparse->GetEntityList();
        }
        return {};
    }
    // ArchetypeStorage<T> に登録された Entity 一覧を取得
    template<typename T>
    std::vector<Entity> GetEntitiesWithArchetypeComponent() {
        std::type_index ti = std::type_index(typeid(T));
        auto it = m_storages.find(ti);
        if (it == m_storages.end()) return {};

        if (auto arche = dynamic_cast<ArchetypeStorage<T>*>(it->second.get())) {
            return arche->GetEntityList();
        }
        return {};
    }
    // SparseStorage<T> に登録された Entity 一覧を取得
    template<typename T>
    std::vector<Entity> GetEntitiesWithSparseComponent() {
        std::type_index ti = std::type_index(typeid(T));
        auto it = m_storages.find(ti);
        if (it == m_storages.end()) return {};

        if (auto sparse = dynamic_cast<SparseStorage<T>*>(it->second.get())) {
            return sparse->GetEntityList(); // ストレージ側で実装必要
        }
        return {};
    }


    // -----------------------------------
    // System 登録
    // -----------------------------------
    void RegisterSystem(std::unique_ptr<ISystem> system) {
        m_systems.emplace_back(std::move(system));
    }

    void InitializeAllSystems() {
        for (auto& sys : m_systems) {
            sys->Initialize();
        }
    }

    void StartAllSystems() {
        for (auto& sys : m_systems) {
            sys->Start();
        }
    }

    void FixedUpdateAllSystems(float fdt) {
        for (auto& sys : m_systems) {
            sys->FixedUpdate(fdt);
        }
    }

    void UpdateAllSystems(float dt) {
        for (auto& sys : m_systems) {
            sys->Update(dt);
        }
    }

    void DrawAllSystems() {
        for (auto& sys : m_systems) {
            sys->Draw();
        }
    }

    // -----------------------------------
    // View クラス用フレンド定義
    // -----------------------------------
    template<typename... Components>
    friend class View;

private:
    // 次に発行する Entity ID
    Entity m_nextID = 1;
    // 再利用可能な ID
    std::vector<Entity> m_recycledIDs;
    // 現在「生存中」の Entity 集合
    std::unordered_set<Entity> m_alive;

    // Entity → マスク
    std::unordered_map<Entity, ComponentMask> m_entityMasks;

    // 型情報から ComponentTypeID へのマップ
    std::unordered_map<std::type_index, ComponentTypeID> m_typeToID;
    ComponentTypeID m_nextComponentTypeID = 0;

    // コンポーネントストレージ本体（Archetype / Sparse を動的に格納）
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> m_storages;

    // 登録されたシステム一覧
    std::vector<std::unique_ptr<ISystem>> m_systems;

    // コンポーネント型 T に対する一意の ID を生成
    template<typename T>
    ComponentTypeID GetComponentTypeID() {
        static ComponentTypeID id = m_nextComponentTypeID++;
        return id;
    }
};

// --------------------------------------------------------------------------------
// View<Ts...> クラス（読み取り専用の簡易ラッパー）
// --------------------------------------------------------------------------------

template<typename... Components>
class View {
public:
    View(EntityRegistry& registry) : m_registry(registry) {
        // Query して対象エンティティ一覧を取得
        m_entities = m_registry.template QueryEntities<Components...>();
    }

    auto begin() { return m_entities.begin(); }
    auto end() { return m_entities.end(); }

    // Entity e に対して各コンポーネントポインタをタプルで返す
    std::tuple<Components*...> GetComponents(Entity e) {
        return std::make_tuple(m_registry.GetComponent<Components>(e)...);
    }

private:
    EntityRegistry& m_registry;
    std::vector<Entity> m_entities;
};

