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

	uint32_t expectedEntityCount = 1024;
	uint32_t expectedTransformCount = 768;
	uint32_t expectedRenderableCount = 512;
	uint32_t expectedCullingCount = 512;
	uint32_t expectedStaticEntityCount = 256;

	uint32_t preallocatedTransformPages = 0;
	uint32_t preallocatedTagPages = 0;

	uint32_t renderPacketReserve = 512;
	uint32_t visibleEntityReserve = 512;
	uint32_t staticBatchReserve = 64;

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

private:
	static constexpr uint32_t RequiredPageCount(uint32_t entityCount) noexcept {
		return entityCount == 0
			? 0
			: ((entityCount - 1) / DirectPagedPageSize) + 1;
	}

	template<typename T>
	static void Read(const YAML::Node& node, const char* key, T& value){
		if(node[key]) value = node[key].as<T>();
	}
};
