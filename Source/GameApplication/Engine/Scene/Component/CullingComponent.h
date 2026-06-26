// =======================================================================
//
// CullingComponent.h
//
// =======================================================================
#pragma once

#include <cstdint>

#include "Backends/myVector3.h"
#include "Backends/yaml-cpp/yaml.h"
#include "Config/SceneStorageConfig.h"
#include "Storage/ComponentStorageStrategy.h"

struct SceneContext;

// Rendering / Physicsライブラリへ依存しないECS共通AABB。
struct EntityAABB {
	Vector3 min = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 max = Vector3(0.0f, 0.0f, 0.0f);

	bool IsValid() const noexcept {
		return min.x <= max.x && min.y <= max.y && min.z <= max.z;
	}
};

// 存在すること自体がCulling有効を表す。
// BoundsとRevisionはCullingSystemが管理する派生データであり、YAMLへ保存しない。
struct CullingComponent {
	EntityAABB localBounds{};
	EntityAABB worldBounds{};
	std::uint64_t sourceRevision = 0;
	std::uint64_t transformRevision = 0;
	bool boundsValid = false;

	YAML::Node encode() const {
		return YAML::Node(YAML::NodeType::Map);
	}

	bool decode(SceneContext*, const YAML::Node&){
		localBounds = {};
		worldBounds = {};
		sourceRevision = 0;
		transformRevision = 0;
		boundsValid = false;
		return true;
	}

	void inspector(SceneContext*){}
};

namespace ECSStorage {

// 値を持つためTag Storageではない。CullingSystemの連続走査を優先する。
template<>
struct ComponentStoragePreference<CullingComponent> {
	static constexpr bool HasExplicitStrategy = true;
	static constexpr ComponentStorageStrategy Strategy =
		ComponentStorageStrategy::Dense;
	static constexpr size_t ExpectedCount =
		SceneStorageConfig::DefaultExpectedCullingCount;
	static constexpr size_t PreallocatedPages = 0;
};

// 通常Component一覧ではなくEntity共通ヘッダーでAttach / Detachする。
template<>
struct IsEntityHeaderComponent<CullingComponent>: std::true_type {};

} // namespace ECSStorage
