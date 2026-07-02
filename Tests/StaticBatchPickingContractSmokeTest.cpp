#include <cassert>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchPickingContract.h"

int main(){
	StaticBatchInstanceData instance;
	instance.entityIndex = 42;
	instance.entityGeneration = 7;
	instance.sceneContextID = 9;
	instance.reserved = 0;

	const StaticBatchPickingParameter parameter =
		StaticBatchPickingContract::Build(instance, 3, 0x12u);

	assert(parameter.SceneID() == 9);
	assert(parameter.ObjectID() == 42);
	assert(parameter.ShaderID() == 3);
	assert(parameter.MaterialFlags() == 0x12u);
	assert(parameter.channels[0] == 9);
	assert(parameter.channels[1] == 42);
	assert(parameter.channels[2] == 3);
	assert(parameter.channels[3] == 0x12u);

	// Picking intentionally resolves the live Entity through SceneID + index.
	// Generation stays in the instance payload for validation/diagnostics and
	// is not written into the existing GBuffer Param contract.
	assert(instance.entityGeneration == 7);
	return 0;
}
