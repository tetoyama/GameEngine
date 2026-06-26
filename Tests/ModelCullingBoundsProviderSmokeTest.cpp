#include <cassert>

#include "Engine/Scene/System/Render/Culling/ModelCullingBoundsProvider.h"

int main(){
	aiScene scene;
	scene.mNumMeshes = 1;
	scene.mMeshes = new aiMesh*[1];
	scene.mMeshes[0] = new aiMesh();

	aiMesh* mesh = scene.mMeshes[0];
	mesh->mNumVertices = 2;
	mesh->mVertices = new aiVector3D[2];
	mesh->mVertices[0] = aiVector3D(-1.0f, -2.0f, -3.0f);
	mesh->mVertices[1] = aiVector3D(4.0f, 5.0f, 6.0f);

	EntityAABB normalBounds;
	assert(ModelCullingBoundsProvider::TryBuildLocalBounds(
		scene,
		false,
		normalBounds
	));
	assert(normalBounds.min.x == -1.0f);
	assert(normalBounds.min.y == -2.0f);
	assert(normalBounds.min.z == -3.0f);
	assert(normalBounds.max.x == 4.0f);
	assert(normalBounds.max.y == 5.0f);
	assert(normalBounds.max.z == 6.0f);

	EntityAABB blenderBounds;
	assert(ModelCullingBoundsProvider::TryBuildLocalBounds(
		scene,
		true,
		blenderBounds
	));
	assert(blenderBounds.min.x == -1.0f);
	assert(blenderBounds.min.y == -6.0f);
	assert(blenderBounds.min.z == -2.0f);
	assert(blenderBounds.max.x == 4.0f);
	assert(blenderBounds.max.y == 3.0f);
	assert(blenderBounds.max.z == 5.0f);

	assert(!ModelCullingBoundsProvider::HasSkinnedMesh(scene));
	mesh->mNumBones = 1;
	mesh->mBones = new aiBone*[1];
	mesh->mBones[0] = new aiBone();
	assert(ModelCullingBoundsProvider::HasSkinnedMesh(scene));
	return 0;
}
