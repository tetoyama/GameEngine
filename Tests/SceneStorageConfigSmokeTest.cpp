#include <cassert>

#include "Engine/Scene/Config/SceneStorageConfig.h"
#include "Engine/Scene/Registry/entityRegistry.h"

int main(){
	SceneStorageConfig config;
	config.expectedEntityCount = 1000;
	config.expectedTransformCount = 513;
	config.Normalize();
	assert(config.ResolveTransformPageCount() == 3);

	EntityRegistry registry;
	registry.Reserve(8);
	assert(registry.GetCapacity() >= 8);
	assert(registry.GetGrowthEventCount() == 0);
	return 0;
}
