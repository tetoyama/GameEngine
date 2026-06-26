#include <cassert>

#include "Engine/Scene/Config/SceneStoragePreloader.h"

int main(){
	SceneStorageConfig loaded;
	assert(SceneStoragePreloader::DecodeFromFile(
		"Tests/Data/SceneStoragePreloader.yaml",
		loaded
	));
	assert(loaded.expectedEntityCount == 2048);
	assert(loaded.expectedTransformCount == 1024);
	assert(loaded.expectedCullingCount == 512);
	assert(loaded.preallocatedTransformPages == 6);

	SceneStorageConfig missing;
	missing.expectedEntityCount = 999;
	assert(!SceneStoragePreloader::DecodeFromFile(
		"Tests/Data/MissingSceneStorage.yaml",
		missing
	));
	assert(missing.expectedEntityCount == 999);
	return 0;
}
