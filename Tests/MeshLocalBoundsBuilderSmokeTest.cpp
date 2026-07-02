#include <array>
#include <cassert>

#include "Engine/Scene/System/Render/Culling/MeshLocalBoundsBuilder.h"

int main(){
	std::array<VERTEX_3D, 4> vertices{};
	vertices[0].Position = {-2.0f, 4.0f, 1.0f};
	vertices[1].Position = {3.0f, -1.0f, 7.0f};
	vertices[2].Position = {0.5f, 2.0f, -5.0f};
	vertices[3].Position = {1.0f, 8.0f, 0.0f};

	MeshData mesh;
	assert(MeshLocalBoundsBuilder::Apply(mesh, vertices));
	assert(mesh.localBoundsValid);
	assert(mesh.localBoundsMin.x == -2.0f);
	assert(mesh.localBoundsMin.y == -1.0f);
	assert(mesh.localBoundsMin.z == -5.0f);
	assert(mesh.localBoundsMax.x == 3.0f);
	assert(mesh.localBoundsMax.y == 8.0f);
	assert(mesh.localBoundsMax.z == 7.0f);
	const auto validRevision = mesh.localBoundsRevision;
	assert(validRevision != 0);

	const std::array<VERTEX_3D, 0> empty{};
	assert(!MeshLocalBoundsBuilder::Apply(mesh, empty));
	assert(!mesh.localBoundsValid);
	assert(mesh.localBoundsRevision != validRevision);
	return 0;
}
