#include <cassert>
#include <string>
#include <vector>

#include "Engine/Scene/Component/Operations/MaterialDescriptorInspector.h"
#include "Engine/Scene/Component/Operations/ModelRendererSubMeshInspector.h"

int main(){
	std::vector<CustomMaterialEntry> materials;
	CustomMaterialEntry* material =
		CustomMaterialCollection::Add(materials, "Inspector Material");
	assert(material);
	assert(material->id == 1);

	MaterialComponent materialComponent;
	materialComponent.materials = materials;
	assert(ModelRendererSubMeshInspector::FirstCustomMaterialID(
		&materialComponent
	) == material->id);
	assert(ModelRendererSubMeshInspector::FirstCustomMaterialID(nullptr) ==
		InvalidCustomMaterialID);
	const std::string displayName =
		ModelRendererSubMeshInspector::CustomMaterialDisplayName(
			materialComponent.materials.front()
		);
	assert(displayName.find("Inspector Material") != std::string::npos);
	assert(displayName.find("ID 1") != std::string::npos);

	std::vector<ModelSubMeshRenderState> states;
	ModelSubMeshRenderState& state =
		ModelRendererSubMeshInspector::EnsureState(states, 17);
	assert(state.subMeshID == 17);
	assert(state.visible);
	assert(state.castShadow);
	assert(state.material.source == SubMeshMaterialSource::ModelDefault);
	return 0;
}
