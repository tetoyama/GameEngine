// =======================================================================
//
// EntityStateComponents.h
//
// =======================================================================
#pragma once

#include "Backends/yaml-cpp/yaml.h"
#include "Storage/ComponentStorageStrategy.h"

struct SceneContext;

// Entity本体へ状態Flagを持たせず、Componentの存在だけで状態を表すTag基底。
// Tagには編集可能な値を持たせず、YAMLにはComponent名だけを保存する。
struct EntityStateTagComponent {
	YAML::Node encode(){
		return YAML::Node(YAML::NodeType::Map);
	}

	bool decode(SceneContext*, const YAML::Node&){
		return true;
	}

	void inspector(SceneContext*){}
};

// 存在するEntityは通常Queryから除外される。
struct DisabledComponent final: EntityStateTagComponent {};

// 存在するEntityはStaticとして扱う。
struct StaticEntityComponent final: EntityStateTagComponent {};

// 存在するEntityは描画対象から除外される。
struct HiddenComponent final: EntityStateTagComponent {};

namespace ECSStorage {

template<>
struct IsTagComponent<DisabledComponent>: std::true_type {};
template<>
struct IsTagComponent<StaticEntityComponent>: std::true_type {};
template<>
struct IsTagComponent<HiddenComponent>: std::true_type {};

template<>
struct IsEntityHeaderComponent<DisabledComponent>: std::true_type {};
template<>
struct IsEntityHeaderComponent<StaticEntityComponent>: std::true_type {};
template<>
struct IsEntityHeaderComponent<HiddenComponent>: std::true_type {};

template<>
struct ExcludeFromDefaultQueries<DisabledComponent>: std::true_type {};

template<>
struct ComponentStoragePreference<DisabledComponent> {
	static constexpr bool HasExplicitStrategy = true;
	static constexpr ComponentStorageStrategy Strategy =
		ComponentStorageStrategy::DirectPaged;
};

template<>
struct ComponentStoragePreference<StaticEntityComponent> {
	static constexpr bool HasExplicitStrategy = true;
	static constexpr ComponentStorageStrategy Strategy =
		ComponentStorageStrategy::DirectPaged;
};

template<>
struct ComponentStoragePreference<HiddenComponent> {
	static constexpr bool HasExplicitStrategy = true;
	static constexpr ComponentStorageStrategy Strategy =
		ComponentStorageStrategy::DirectPaged;
};

} // namespace ECSStorage
