#include <cassert>

#include "Engine/Scene/Config/SceneStorageConfig.h"

int main(){
	SceneStorageConfig config;
	config.expectedEntityCount = 1000;
	config.expectedTransformCount = 513;
	config.Normalize();
	assert(config.ResolveTransformPageCount() == 3);
	return 0;
}
