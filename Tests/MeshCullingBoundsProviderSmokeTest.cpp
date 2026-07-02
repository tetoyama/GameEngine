#include <cassert>

#include "Engine/Scene/System/Render/Culling/MeshCullingBoundsProvider.h"

int main(){
	MeshRendererComponent renderer;
	EntityAABB bounds;

	assert(!MeshCullingBoundsProvider::TryBuildLocalBounds(renderer, bounds));

	renderer.mesh.SetLocalBounds(
		Vector3(-2.0f, -1.0f, -3.0f),
		Vector3(4.0f, 5.0f, 6.0f)
	);
	assert(MeshCullingBoundsProvider::TryBuildLocalBounds(renderer, bounds));
	assert(bounds.min.x == -2.0f);
	assert(bounds.min.y == -1.0f);
	assert(bounds.min.z == -3.0f);
	assert(bounds.max.x == 4.0f);
	assert(bounds.max.y == 5.0f);
	assert(bounds.max.z == 6.0f);

	const auto firstRevision =
		MeshCullingBoundsProvider::MakeSourceRevision(renderer);
	renderer.mesh.meshCount = 12;
	const auto countRevision =
		MeshCullingBoundsProvider::MakeSourceRevision(renderer);
	assert(firstRevision != countRevision);

	renderer.mesh.SetLocalBounds(
		Vector3(-4.0f, -3.0f, -2.0f),
		Vector3(1.0f, 2.0f, 3.0f)
	);
	const auto boundsRevision =
		MeshCullingBoundsProvider::MakeSourceRevision(renderer);
	assert(countRevision != boundsRevision);

	renderer.mesh.SetLocalBounds(
		Vector3(1.0f, 0.0f, 0.0f),
		Vector3(-1.0f, 1.0f, 1.0f)
	);
	assert(!MeshCullingBoundsProvider::TryBuildLocalBounds(renderer, bounds));

	renderer.mesh.ClearLocalBounds();
	assert(!MeshCullingBoundsProvider::TryBuildLocalBounds(renderer, bounds));
	return 0;
}
