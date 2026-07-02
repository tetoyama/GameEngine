// =======================================================================
//
// SceneStorageConfig.h
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "Backends/yaml-cpp/yaml.h"

struct SceneStorageConfig {
	static constexpr uint32_t DirectPagedPageSize = 256;

	static constexpr uint32_t DefaultExpectedEntityCount = 1024;
	static constexpr uint32_t DefaultExpectedTransformCount = 768;
	static constexpr uint32_t DefaultExpectedRenderableCount = 512;
	static constexpr uint32_t DefaultExpectedCullingCount = 512;
	static constexpr uint32_t DefaultExpectedStaticEntityCount = 256;
	static constexpr uint32_t DefaultRenderPacketReserve = 512;
	static constexpr uint32_t DefaultVisibleEntityReserve = 512;
	static constexpr uint32_t DefaultStaticBatchReserve = 64;

	uint32_t expectedEntityCount = DefaultExpectedEntityCount;
	uint32_t expectedTransformCount = DefaultExpectedTransformCount;
	uint32_t expectedRenderableCount = DefaultExpectedRenderableCount;
	uint32_t expectedCullingCount = DefaultExpectedCullingCount;
	uint32_t expectedStaticEntityCount = DefaultExpectedStaticEntityCount;

	uint32_t preallocatedTransformPages = 0;
	uint32_t preallocatedTagPages = 0;

	uint32_t renderPacketReserve = DefaultRenderPacketReserve;
	uint32_t visibleEntityReserve = DefaultVisibleEntityReserve;
	uint32_t staticBatchReserve = DefaultStaticBatchReserve;

	bool allowRuntimeGrowth = true;
	bool logCapacityGrowth = true;

	uint32_t ResolveTransformPageCount() const noexcept {
		if(preallocatedTransformPages > 0){
			return preallocatedTransformPages;
		}
		return RequiredPageCount(expectedTransformCount);
	}

	uint32_t ResolveTagPageCount() const noexcept {
		if(preallocatedTagPages > 0){
			return preallocatedTagPages;
		}
		return RequiredPageCount(expectedStaticEntityCount);
	}

	void Normalize() noexcept {
		expectedEntityCount = (std::max)(expectedEntityCount, 1u);
		expectedTransformCount =
			(std::min)(expectedTransformCount, expectedEntityCount);
		expectedRenderableCount =
			(std::min)(expectedRenderableCount, expectedEntityCount);
		expectedCullingCount =
			(std::min)(expectedCullingCount, expectedEntityCount);
		expectedStaticEntityCount =
			(std::min)(expectedStaticEntityCount, expectedEntityCount);
	}

	YAML::Node Encode() const {
		YAML::Node node;
		node["ExpectedEntities"] = expectedEntityCount;
		node["ExpectedTransforms"] = expectedTransformCount;
		node["ExpectedRenderables"] = expectedRenderableCount;
		node["ExpectedCullingComponents"] = expectedCullingCount;
		node["ExpectedStaticEntities"] = expectedStaticEntityCount;
		node["PreallocatedTransformPages"] = preallocatedTransformPages;
		node["PreallocatedTagPages"] = preallocatedTagPages;
		node["RenderPacketReserve"] = renderPacketReserve;
		node["VisibleEntityReserve"] = visibleEntityReserve;
		node["StaticBatchReserve"] = staticBatchReserve;
		node["AllowRuntimeGrowth"] = allowRuntimeGrowth;
		node["LogCapacityGrowth"] = logCapacityGrowth;
		return node;
	}

	void Decode(const YAML::Node& node){
		if(!node || !node.IsMap()) return;

		Read(node, "ExpectedEntities", expectedEntityCount);
		Read(node, "ExpectedTransforms", expectedTransformCount);
		Read(node, "ExpectedRenderables", expectedRenderableCount);
		Read(node, "ExpectedCullingComponents", expectedCullingCount);
		Read(node, "ExpectedStaticEntities", expectedStaticEntityCount);
		Read(node, "PreallocatedTransformPages", preallocatedTransformPages);
		Read(node, "PreallocatedTagPages", preallocatedTagPages);
		Read(node, "RenderPacketReserve", renderPacketReserve);
		Read(node, "VisibleEntityReserve", visibleEntityReserve);
		Read(node, "StaticBatchReserve", staticBatchReserve);
		Read(node, "AllowRuntimeGrowth", allowRuntimeGrowth);
		Read(node, "LogCapacityGrowth", logCapacityGrowth);
		Normalize();
	}

	static constexpr uint32_t RequiredPageCount(uint32_t entityCount) noexcept {
		return entityCount == 0
			? 0
			: ((entityCount - 1) / DirectPagedPageSize) + 1;
	}

private:
	template<typename T>
	static void Read(const YAML::Node& node, const char* key, T& value){
		if(node[key]) value = node[key].as<T>();
	}
};
