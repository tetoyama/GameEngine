// =======================================================================
//
// componentRegistry.h
//
// =======================================================================
#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "entityRegistry.h"
#include "Scene.h"
#include "Interface/IComponent.h"
#include "Interface/IComponentStorage.h"
#include "Query/ComponentQueryView.h"

struct ComponentOperations {
	std::string name;
	std::function<YAML::Node(void*)> encode;
	std::function<bool(void*, SceneContext*, const YAML::Node&)> decode;
	std::function<void(void*, SceneContext*)> inspector;
};

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
	using YAMLCreator =
		std::function<ComponentView(Entity, const YAML::Node&)>;

	explicit ComponentRegistry(
		EntityRegistry* entityManager,
		SceneContext* context
	)
		: m_context(context),
		  m_entityManager(entityManager){}

	template<typename T>
	void RegisterYAMLComponent(
		const std::string& name,
		bool useDensePool = false
	){
		if(m_yamlFactory.contains(name)) return;

		RegisterComponent<T>(useDensePool);

		const ComponentTypeID typeID = ComponentType::Get<T>();
		m_componentOperations[typeID] = {
			name,
			[](void* raw) -> YAML::Node {
				return static_cast<T*>(raw)->encode();
			},
			[](void* raw, SceneContext* context, const YAML::Node& node) -> bool {
				return static_cast<T*>(raw)->decode(context, node);
			},
			[](void* raw, SceneContext* context){
				static_cast<T*>(raw)->inspector(context);
			}
		};

		m_yamlFactory[name] = [this, typeID](
			Entity entity,
			const YAML::Node& node
		) -> ComponentView {
			T* component = AddComponent<T>(entity);
			if(!component) return {};
			DecodeComponent(typeID, component, node);
			return {typeID, component};
		};

		m_addComponentFuncs[name] = [this](Entity entity){
			AddComponent<T>(entity);
		};

		const std::string runtimeTypeName = typeid(T).name();
		m_addDefaultComponentByRuntimeTypeName[runtimeTypeName] =
			[this](Entity entity){ AddComponent<T>(entity); };
		m_removeComponentByRuntimeTypeName[runtimeTypeName] =
			[this](Entity entity){ RemoveComponent<T>(entity); };

		m_nameToComponentID[name] = typeID;
		m_componentIDToName[typeID] = name;
		m_typeToID[typeid(T)] = typeID;
		m_idToTypeIndex.emplace(typeID, typeid(T));
		m_componentRegistrationOrder.push_back(name);

		if constexpr(std::is_base_of_v<IComponent, T>){
			m_polymorphicAdapters[typeid(T)] = [](void* raw) -> IComponent* {
				return static_cast<T*>(raw);
			};
		}
	}

	ComponentView CreateFromYAML(
		const std::string& name,
		Entity entity,
		const YAML::Node& node
	){
		auto iterator = m_yamlFactory.find(name);
		return iterator != m_yamlFactory.end()
			? iterator->second(entity, node)
			: ComponentView{};
	}

	template<typename T, typename... Args>
	T* AddComponent(Entity entity, Args&&... args){
		assert(m_entityManager &&
			m_entityManager->IsAlive(entity) &&
			"AddComponent: Entity is not alive");

		const std::type_index typeIndex(typeid(T));
		if(!m_storages.contains(typeIndex)) RegisterComponent<T>();

		T component(std::forward<Args>(args)...);
		IComponentStorage* storage = m_storages[typeIndex].get();
		if(auto* dense = dynamic_cast<DenseComponentPool<T>*>(storage)){
			dense->Add(entity, std::move(component));
		} else if(auto* sparse = dynamic_cast<SparseStorage<T>*>(storage)){
			sparse->Add(entity, std::move(component));
		} else {
			assert(false && "Unknown component storage type");
			return nullptr;
		}

		m_entityMasks[entity].set(ComponentType::Get<T>());
		return GetComponent<T>(entity);
	}

	template<typename T>
	T* GetComponent(Entity entity){
		if(!m_entityManager || !m_entityManager->IsAlive(entity)) return nullptr;
		auto iterator = m_storages.find(std::type_index(typeid(T)));
		return iterator != m_storages.end()
			? static_cast<T*>(iterator->second->GetRaw(entity))
			: nullptr;
	}

	template<typename T>
	const T* GetComponent(Entity entity) const {
		if(!m_entityManager || !m_entityManager->IsAlive(entity)) return nullptr;
		auto iterator = m_storages.find(std::type_index(typeid(T)));
		return iterator != m_storages.end()
			? static_cast<const T*>(iterator->second->GetRaw(entity))
			: nullptr;
	}

	ComponentView GetComponentByID(Entity entity, ComponentTypeID typeID){
		auto typeIterator = m_idToTypeIndex.find(typeID);
		if(typeIterator == m_idToTypeIndex.end()) return {};
		auto storageIterator = m_storages.find(typeIterator->second);
		if(storageIterator == m_storages.end()) return {};
		void* data = storageIterator->second->GetRaw(entity);
		return data ? ComponentView{typeID, data} : ComponentView{};
	}

	ConstComponentView GetComponentByID(
		Entity entity,
		ComponentTypeID typeID
	) const {
		auto typeIterator = m_idToTypeIndex.find(typeID);
		if(typeIterator == m_idToTypeIndex.end()) return {};
		auto storageIterator = m_storages.find(typeIterator->second);
		if(storageIterator == m_storages.end()) return {};
		const void* data = storageIterator->second->GetRaw(entity);
		return data ? ConstComponentView{typeID, data} : ConstComponentView{};
	}

	template<typename TBase>
	std::vector<std::pair<Entity, TBase*>> GetAllBaseComponents(){
		std::vector<std::pair<Entity, TBase*>> result;
		for(const auto& [typeIndex, storage] : m_storages){
			auto adapterIterator = m_polymorphicAdapters.find(typeIndex);
			if(adapterIterator == m_polymorphicAdapters.end()) continue;
			for(Entity entity : storage->GetEntityList()){
				void* raw = storage->GetRaw(entity);
				IComponent* polymorphic = raw
					? adapterIterator->second(raw)
					: nullptr;
				if(auto* component = dynamic_cast<TBase*>(polymorphic)){
					result.emplace_back(entity, component);
				}
			}
		}
		return result;
	}

	template<typename T>
	uint32_t GetComponentGeneration(Entity entity) const {
		auto iterator = m_storages.find(std::type_index(typeid(T)));
		return iterator != m_storages.end()
			? iterator->second->GetComponentGeneration(entity)
			: 0;
	}

	template<typename T>
	uint64_t GetStructureVersion() const {
		auto iterator = m_storages.find(std::type_index(typeid(T)));
		return iterator != m_storages.end()
			? iterator->second->GetStructureVersion()
			: 0;
	}

	template<typename T>
	bool HasComponent(Entity entity) const {
		if(!m_entityManager || !m_entityManager->IsAlive(entity)) return false;
		auto maskIterator = m_entityMasks.find(entity);
		return maskIterator != m_entityMasks.end() &&
			maskIterator->second.test(ComponentType::Get<T>());
	}

	template<typename T>
	void RemoveComponent(Entity entity){
		if(!m_entityManager || !m_entityManager->IsAlive(entity)) return;
		auto iterator = m_storages.find(std::type_index(typeid(T)));
		if(iterator == m_storages.end()) return;
		iterator->second->Remove(entity);
		auto maskIterator = m_entityMasks.find(entity);
		if(maskIterator != m_entityMasks.end()){
			maskIterator->second.reset(ComponentType::Get<T>());
		}
	}

	void RemoveComponentByID(Entity entity, ComponentTypeID typeID){
		auto typeIterator = m_idToTypeIndex.find(typeID);
		if(typeIterator == m_idToTypeIndex.end()) return;
		auto storageIterator = m_storages.find(typeIterator->second);
		if(storageIterator == m_storages.end()) return;
		storageIterator->second->Remove(entity);
		auto maskIterator = m_entityMasks.find(entity);
		if(maskIterator != m_entityMasks.end()) maskIterator->second.reset(typeID);
	}

	void OnEntityDestroyed(Entity entity){
		for(auto& [typeIndex, storage] : m_storages){
			(void)typeIndex;
			storage->Remove(entity);
		}
		m_entityMasks.erase(entity);
	}

	template<typename... Accesses>
	ECSQuery::ComponentQueryView<Accesses...> Query() const {
		ComponentMask required;
		(required.set(
			ComponentType::Get<typename ECSQuery::ComponentType<Accesses>>()
		), ...);

		return ECSQuery::ComponentQueryView<Accesses...>(
			m_entityManager->GetAllAlive(),
			m_entityMasks,
			required
		);
	}

	template<typename... Components>
	auto ReadQuery() const {
		return Query<ECSQuery::Read<Components>...>();
	}

	template<typename... Components>
	std::vector<Entity> QueryEntities(){
		std::vector<Entity> result;
		const auto& aliveEntities = m_entityManager->GetAllAlive();
		ComponentMask required;
		(required.set(ComponentType::Get<Components>()), ...);
		result.reserve(aliveEntities.size());
		for(Entity entity : aliveEntities){
			auto iterator = m_entityMasks.find(entity);
			if(iterator != m_entityMasks.end() &&
				(iterator->second & required) == required){
				result.push_back(entity);
			}
		}
		return result;
	}

	template<typename T>
	std::vector<Entity> FindEntitiesWithComponent() const {
		auto iterator = m_storages.find(std::type_index(typeid(T)));
		return iterator != m_storages.end()
			? iterator->second->GetEntityList()
			: std::vector<Entity>{};
	}

	template<typename T>
	std::span<T> GetDenseComponentSpan(){
		auto iterator = m_storages.find(std::type_index(typeid(T)));
		if(iterator == m_storages.end()) return {};
		auto* dense = dynamic_cast<DenseComponentPool<T>*>(iterator->second.get());
		return dense ? dense->Components() : std::span<T>{};
	}

	template<typename T>
	std::span<const Entity> GetDenseEntitySpan() const {
		auto iterator = m_storages.find(std::type_index(typeid(T)));
		if(iterator == m_storages.end()) return {};
		auto* dense = dynamic_cast<const DenseComponentPool<T>*>(
			iterator->second.get()
		);
		return dense ? dense->Entities() : std::span<const Entity>{};
	}

	std::vector<ComponentView> GetAllComponentViewsOfEntitySorted(Entity entity){
		std::vector<ComponentView> components;
		if(!m_entityManager || !m_entityManager->IsAlive(entity)) return components;
		components.reserve(m_componentRegistrationOrder.size());
		for(const std::string& name : m_componentRegistrationOrder){
			auto idIterator = m_nameToComponentID.find(name);
			if(idIterator == m_nameToComponentID.end()) continue;
			ComponentView view = GetComponentByID(entity, idIterator->second);
			if(view) components.push_back(view);
		}
		return components;
	}

	std::vector<ComponentView> GetAllComponentsOfEntitySorted(Entity entity){
		return GetAllComponentViewsOfEntitySorted(entity);
	}

	std::string GetComponentName(ComponentTypeID typeID) const {
		auto iterator = m_componentOperations.find(typeID);
		return iterator != m_componentOperations.end()
			? iterator->second.name
			: std::string{};
	}

	std::string GetComponentName(ComponentView view) const {
		return GetComponentName(view.typeID);
	}

	YAML::Node EncodeComponent(ComponentView view) const {
		if(!view) return {};
		auto iterator = m_componentOperations.find(view.typeID);
		return iterator != m_componentOperations.end() && iterator->second.encode
			? iterator->second.encode(view.data)
			: YAML::Node{};
	}

	YAML::Node EncodeComponent(ConstComponentView view) const {
		if(!view) return {};
		auto iterator = m_componentOperations.find(view.typeID);
		return iterator != m_componentOperations.end() && iterator->second.encode
			? iterator->second.encode(const_cast<void*>(view.data))
			: YAML::Node{};
	}

	bool DecodeComponent(
		ComponentTypeID typeID,
		void* component,
		const YAML::Node& node
	) const {
		if(!component) return false;
		auto iterator = m_componentOperations.find(typeID);
		return iterator != m_componentOperations.end() && iterator->second.decode
			? iterator->second.decode(component, m_context, node)
			: false;
	}

	bool InspectComponent(ComponentView view, SceneContext* context) const {
		if(!view) return false;
		auto iterator = m_componentOperations.find(view.typeID);
		if(iterator == m_componentOperations.end() || !iterator->second.inspector){
			return false;
		}
		iterator->second.inspector(view.data, context);
		return true;
	}

	template<typename T, typename EncodeFn, typename DecodeFn, typename InspectorFn>
	void SetComponentOperations(
		EncodeFn&& encode,
		DecodeFn&& decode,
		InspectorFn&& inspector
	){
		const ComponentTypeID typeID = ComponentType::Get<T>();
		auto iterator = m_componentOperations.find(typeID);
		if(iterator == m_componentOperations.end()) return;
		iterator->second.encode =
			[function = std::forward<EncodeFn>(encode)](void* raw) mutable {
				return function(*static_cast<const T*>(raw));
			};
		iterator->second.decode =
			[function = std::forward<DecodeFn>(decode)](
				void* raw,
				SceneContext* context,
				const YAML::Node& node
			) mutable {
				return function(*static_cast<T*>(raw), context, node);
			};
		iterator->second.inspector =
			[function = std::forward<InspectorFn>(inspector)](
				void* raw,
				SceneContext* context
			) mutable {
				function(*static_cast<T*>(raw), context);
			};
	}

	bool AddDefaultComponentByRuntimeTypeName(
		Entity entity,
		const std::string& runtimeTypeName
	){
		if(!m_entityManager || !m_entityManager->IsAlive(entity)) return false;
		auto iterator = m_addDefaultComponentByRuntimeTypeName.find(runtimeTypeName);
		if(iterator == m_addDefaultComponentByRuntimeTypeName.end()) return false;
		iterator->second(entity);
		return true;
	}

	bool RemoveComponentByRuntimeTypeName(
		Entity entity,
		const std::string& runtimeTypeName
	){
		if(!m_entityManager || !m_entityManager->IsAlive(entity)) return false;
		auto iterator = m_removeComponentByRuntimeTypeName.find(runtimeTypeName);
		if(iterator == m_removeComponentByRuntimeTypeName.end()) return false;
		iterator->second(entity);
		return true;
	}

	const std::unordered_map<std::string, std::function<void(Entity)>>&
	GetAddableComponentList() const {
		return m_addComponentFuncs;
	}

	const std::unordered_map<Entity, ComponentMask>& GetEntityMasks() const {
		return m_entityMasks;
	}

	ComponentTypeID GetComponentIDByName(const std::string& name) const {
		auto iterator = m_nameToComponentID.find(name);
		return iterator != m_nameToComponentID.end()
			? iterator->second
			: INVALID_COMPONENT_TYPE_ID;
	}

	ComponentTypeID GetComponentIDByTypeIndex(
		const std::type_index& typeIndex
	) const {
		auto iterator = m_typeToID.find(typeIndex);
		return iterator != m_typeToID.end()
			? iterator->second
			: INVALID_COMPONENT_TYPE_ID;
	}

	const std::unordered_map<ComponentTypeID, std::string>&
	GetComponentIDToNameMap() const {
		return m_componentIDToName;
	}

private:
	template<typename T>
	void RegisterComponent(bool useDensePool = false){
		const std::type_index typeIndex(typeid(T));
		if(m_storages.contains(typeIndex)) return;
		if(useDensePool){
			m_storages[typeIndex] = std::make_unique<DenseComponentPool<T>>();
		} else {
			m_storages[typeIndex] = std::make_unique<SparseStorage<T>>();
		}
	}

	SceneContext* m_context = nullptr;
	EntityRegistry* m_entityManager = nullptr;
	std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> m_storages;
	std::unordered_map<std::type_index, ComponentTypeID> m_typeToID;
	std::unordered_map<ComponentTypeID, std::type_index> m_idToTypeIndex;
	std::unordered_map<Entity, ComponentMask> m_entityMasks;
	std::unordered_map<ComponentTypeID, ComponentOperations> m_componentOperations;
	std::unordered_map<std::string, YAMLCreator> m_yamlFactory;
	std::unordered_map<std::string, std::function<void(Entity)>> m_addComponentFuncs;
	std::unordered_map<std::string, std::function<void(Entity)>>
		m_addDefaultComponentByRuntimeTypeName;
	std::unordered_map<std::string, std::function<void(Entity)>>
		m_removeComponentByRuntimeTypeName;
	std::unordered_map<std::string, ComponentTypeID> m_nameToComponentID;
	std::unordered_map<ComponentTypeID, std::string> m_componentIDToName;
	std::unordered_map<std::type_index, std::function<IComponent*(void*)>>
		m_polymorphicAdapters;
	std::vector<std::string> m_componentRegistrationOrder;
};
