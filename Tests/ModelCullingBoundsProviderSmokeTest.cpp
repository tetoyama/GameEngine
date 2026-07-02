#include <array>
#include <cassert>
#include <span>

#include "Engine/Scene/System/Render/Culling/ModelCullingBoundsProvider.h"

int main(){
	const std::array<aiVector3D, 3> positions{
		aiVector3D(-1.0f, -2.0f, -3.0f),
		aiVector3D(4.0f, 5.0f, 6.0f),
		aiVector3D(2.0f, -4.0f, 1.0f)
	};

	EntityAABB normalBounds;
	assert(ModelCullingBoundsProvider::TryBuildLocalBoundsFromPositions(
		std::span<const aiVector3D>(positions),
		false,
		normalBounds
	));
	assert(normalBounds.min.x == -1.0f);
	assert(normalBounds.min.y == -4.0f);
	assert(normalBounds.min.z == -3.0f);
	assert(normalBounds.max.x == 4.0f);
	assert(normalBounds.max.y == 5.0f);
	assert(normalBounds.max.z == 6.0f);

	EntityAABB blenderBounds;
	assert(ModelCullingBoundsProvider::TryBuildLocalBoundsFromPositions(
		std::span<const aiVector3D>(positions),
		true,
		blenderBounds
	));
	assert(blenderBounds.min.x == -1.0f);
	assert(blenderBounds.min.y == -6.0f);
	assert(blenderBounds.min.z == -4.0f);
	assert(blenderBounds.max.x == 4.0f);
	assert(blenderBounds.max.y == 3.0f);
	assert(blenderBounds.max.z == 5.0f);

	EntityAABB emptyBounds;
	assert(!ModelCullingBoundsProvider::TryBuildLocalBoundsFromPositions(
		std::span<const aiVector3D>{},
		false,
		emptyBounds
	));
	return 0;
}
