#include <cassert>
#include <vector>

#include "Engine/Scene/Component/Operations/ModelSubMeshStateSynchronization.h"

int main(){
	std::vector<ModelSubMeshDefinition> definitions(3);
	definitions[0].id = 11;
	definitions[1].id = 22;
	definitions[2].id = 33;

	std::vector<ModelSubMeshRenderState> states;
	states.push_back({
		22,
		false,
		true,
		{SubMeshMaterialSource::CustomMaterial, 42}
	});
	states.push_back({
		22,
		true,
		false,
		{SubMeshMaterialSource::ModelDefault, InvalidCustomMaterialID}
	});
	states.push_back({
		44,
		false,
		false,
		{SubMeshMaterialSource::CustomMaterial, 7}
	});
	states.push_back({
		33,
		true,
		true,
		{SubMeshMaterialSource::CustomMaterial, InvalidCustomMaterialID}
	});
	states.push_back({});

	ModelSubMeshStateSynchronization::Synchronize(states, definitions);
	assert(states.size() == 3);
	assert(states[0].subMeshID == 11);
	assert(ModelSubMeshStateSynchronization::IsDefaultState(states[0]));
	assert(states[1].subMeshID == 22);
	assert(!states[1].visible);
	assert(states[1].castShadow);
	assert(states[1].material.source == SubMeshMaterialSource::CustomMaterial);
	assert(states[1].material.customMaterialID == 42);
	assert(states[2].subMeshID == 33);
	assert(ModelSubMeshStateSynchronization::IsDefaultState(states[2]));

	std::vector<ModelSubMeshDefinition> reimported(3);
	reimported[0].id = 33;
	reimported[1].id = 22;
	reimported[2].id = 55;
	ModelSubMeshStateSynchronization::Synchronize(states, reimported);
	assert(states.size() == 3);
	assert(states[0].subMeshID == 33);
	assert(ModelSubMeshStateSynchronization::IsDefaultState(states[0]));
	assert(states[1].subMeshID == 22);
	assert(!states[1].visible);
	assert(states[1].material.customMaterialID == 42);
	assert(states[2].subMeshID == 55);
	assert(ModelSubMeshStateSynchronization::IsDefaultState(states[2]));
	return 0;
}
