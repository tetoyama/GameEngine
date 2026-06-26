#include <cassert>
#include <cstdint>

#include "Engine/Scene/System/Render/Culling/TerrainCullingBoundsProvider.h"

int main(){
	TerrainComponent terrain;
	terrain.Scale = 2;
	terrain.CurrentScale = 2;
	terrain.HeightMap = {
		0.0f, 1.0f, 2.0f,
		-1.0f, 0.5f, 3.0f,
		0.25f, 1.5f, 2.5f
	};

	EntityAABB bounds;
	assert(TerrainCullingBoundsProvider::TryBuildLocalBounds(terrain, bounds));
	assert(bounds.min.x == -0.5f);
	assert(bounds.min.y == -1.0f);
	assert(bounds.min.z == -0.5f);
	assert(bounds.max.x == 0.5f);
	assert(bounds.max.y == 3.0f);
	assert(bounds.max.z == 0.5f);

	const std::uint64_t initialRevision =
		TerrainCullingBoundsProvider::MakeSourceRevision(terrain);
	terrain.CurrentScale = 0;
	const std::uint64_t dirtyRevision =
		TerrainCullingBoundsProvider::MakeSourceRevision(terrain);
	assert(initialRevision != dirtyRevision);

	TerrainComponent incomplete;
	incomplete.Scale = 4;
	incomplete.CurrentScale = 4;
	incomplete.HeightMap = {10.0f, -5.0f};
	EntityAABB flatBounds;
	assert(TerrainCullingBoundsProvider::TryBuildLocalBounds(incomplete, flatBounds));
	assert(flatBounds.min.x == -0.5f);
	assert(flatBounds.min.y == 0.0f);
	assert(flatBounds.min.z == -0.5f);
	assert(flatBounds.max.x == 0.5f);
	assert(flatBounds.max.y == 0.0f);
	assert(flatBounds.max.z == 0.5f);

	TerrainComponent invalid;
	invalid.Scale = 0;
	EntityAABB invalidBounds;
	assert(!TerrainCullingBoundsProvider::TryBuildLocalBounds(invalid, invalidBounds));
	return 0;
}
