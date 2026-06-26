// =======================================================================
//
// MeshCullingBoundsProvider.h
//
// =======================================================================
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "Scene/Component/CullingComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Registry/componentRegistry.h"

namespace MeshCullingBoundsProvider {

inline bool TryBuildLocalBounds(
	const MeshRendererComponent& renderer,
	EntityAABB& outBounds
) noexcept {
	const MeshData& mesh = renderer.mesh;
	if(!mesh.localBoundsValid) return false;

	const Vector3& minimum = mesh.localBoundsMin;
	const Vector3& maximum = mesh.localBoundsMax;
	if(!std::isfinite(minimum.x) ||
		!std::isfinite(minimum.y) ||
		!std::isfinite(minimum.z) ||
		!std::isfinite(maximum.x) ||
		!std::isfinite(maximum.y) ||
		!std::isfinite(maximum.z)){
		return false;
	}
	if(minimum.x > maximum.x ||
		minimum.y > maximum.y ||
		minimum.z > maximum.z){
		return false;
	}

	outBounds.min = minimum;
	outBounds.max = maximum;
	return true;
}

inline std::uint64_t MakeSourceRevision(
	const MeshRendererComponent& renderer
) noexcept {
	const MeshData& mesh = renderer.mesh;
	std::uint64_t revision = 0x4d455348ull;
	const auto combine = [&revision](std::uint64_t value){
		revision ^= value + 0x9e3779b97f4a7c15ull +
			(revision << 6) + (revision >> 2);
	};
	combine(mesh.localBoundsRevision);
	combine(static_cast<std::uint64_t>(
		static_cast<std::uint32_t>(mesh.meshCount)
	));
	combine(static_cast<std::uint64_t>(
		static_cast<std::uint32_t>(mesh.indexCount)
	));
	return revision == 0 ? 1 : revision;
}

struct UpdateResult {
	size_t visited = 0;
	size_t updated = 0;
	size_t unavailable = 0;
	size_t skippedHigherPrioritySource = 0;
};

// Bounds source priority: Model > Mesh.
inline UpdateResult UpdateScene(ComponentRegistry& components){
	UpdateResult result;
	const auto entities =
		components.FindEntitiesWithComponent<CullingComponent>();

	for(Entity entity : entities){
		MeshRendererComponent* renderer =
			components.GetComponent<MeshRendererComponent>(entity);
		if(!renderer) continue;

		++result.visited;
		if(components.GetComponent<ModelRendererComponent>(entity)){
			++result.skippedHigherPrioritySource;
			continue;
		}

		EntityAABB localBounds;
		if(!TryBuildLocalBounds(*renderer, localBounds)){
			++result.unavailable;
			continue;
		}

		CullingComponent* culling =
			components.GetComponent<CullingComponent>(entity);
		if(!culling){
			++result.unavailable;
			continue;
		}

		const std::uint64_t revision = MakeSourceRevision(*renderer);
		if(culling->sourceRevision == revision) continue;

		culling->localBounds = localBounds;
		culling->sourceRevision = revision;
		culling->boundsValid = false;
		++result.updated;
	}
	return result;
}

} // namespace MeshCullingBoundsProvider
