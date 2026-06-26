// =======================================================================
//
// TerrainCullingBoundsProvider.h
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "Scene/Component/CullingComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Component/terrainComponent.h"
#include "Scene/Registry/componentRegistry.h"
#include "MeshCullingBoundsProvider.h"

namespace TerrainCullingBoundsProvider {

inline bool TryBuildLocalBounds(
	const TerrainComponent& terrain,
	EntityAABB& outBounds
) noexcept {
	if(terrain.Scale <= 0) return false;

	const size_t side = static_cast<size_t>(terrain.Scale) + 1;
	if(side > (std::numeric_limits<size_t>::max)() / side){
		return false;
	}
	const size_t expectedVertexCount = side * side;

	float minimumHeight = 0.0f;
	float maximumHeight = 0.0f;
	if(terrain.HeightMap.size() >= expectedVertexCount){
		auto begin = terrain.HeightMap.begin();
		auto end = begin + static_cast<std::ptrdiff_t>(expectedVertexCount);
		const auto [minimum, maximum] = std::minmax_element(begin, end);
		minimumHeight = *minimum;
		maximumHeight = *maximum;
	}

	outBounds.min = Vector3(-0.5f, minimumHeight, -0.5f);
	outBounds.max = Vector3(0.5f, maximumHeight, 0.5f);
	return true;
}

inline std::uint64_t MakeSourceRevision(
	const TerrainComponent& terrain
) noexcept {
	std::uint64_t revision = 0x5445525241494eull;
	const auto combine = [&revision](std::uint64_t value){
		revision ^= value + 0x9e3779b97f4a7c15ull +
			(revision << 6) + (revision >> 2);
	};
	combine(static_cast<std::uint64_t>(
		static_cast<std::uint32_t>(terrain.Scale)
	));
	combine(static_cast<std::uint64_t>(
		static_cast<std::uint32_t>(terrain.CurrentScale)
	));
	combine(static_cast<std::uint64_t>(terrain.HeightMap.size()));
	combine(static_cast<std::uint64_t>(
		reinterpret_cast<std::uintptr_t>(terrain.HeightMap.data())
	));
	return revision == 0 ? 1 : revision;
}

struct UpdateResult {
	size_t visited = 0;
	size_t updated = 0;
	size_t unavailable = 0;
	size_t skippedHigherPrioritySource = 0;
};

// Bounds source priority: Model > valid Mesh > Terrain.
inline UpdateResult UpdateScene(ComponentRegistry& components){
	UpdateResult result;
	const auto entities =
		components.FindEntitiesWithComponent<CullingComponent>();

	for(Entity entity : entities){
		TerrainComponent* terrain =
			components.GetComponent<TerrainComponent>(entity);
		if(!terrain) continue;

		++result.visited;
		if(components.GetComponent<ModelRendererComponent>(entity)){
			++result.skippedHigherPrioritySource;
			continue;
		}

		if(MeshRendererComponent* mesh =
			components.GetComponent<MeshRendererComponent>(entity)){
			EntityAABB meshBounds;
			if(MeshCullingBoundsProvider::TryBuildLocalBounds(*mesh, meshBounds)){
				++result.skippedHigherPrioritySource;
				continue;
			}
		}

		CullingComponent* culling =
			components.GetComponent<CullingComponent>(entity);
		if(!culling){
			++result.unavailable;
			continue;
		}

		const std::uint64_t revision = MakeSourceRevision(*terrain);
		if(culling->sourceRevision == revision) continue;

		EntityAABB localBounds;
		if(!TryBuildLocalBounds(*terrain, localBounds)){
			culling->sourceRevision = 0;
			culling->boundsValid = false;
			++result.unavailable;
			continue;
		}

		culling->localBounds = localBounds;
		culling->sourceRevision = revision;
		culling->boundsValid = false;
		++result.updated;
	}
	return result;
}

} // namespace TerrainCullingBoundsProvider
