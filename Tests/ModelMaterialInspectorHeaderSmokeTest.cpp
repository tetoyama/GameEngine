#include <cassert>
#include <string>
#include <vector>

#include "Engine/Scene/Component/Operations/MaterialDescriptorInspector.h"
#include "Engine/Scene/Component/materialComponent.h"
#include "Engine/Scene/Component/modelRendererComponent.h"

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

	// Play開始前に保存された旧Temp SceneにはMaterialsが存在しない。
	// 欠落const NodeをInvalidNodeのままDecoderへ渡さず、空Collectionとして
	// 復元できることを実行時に確認する。
	YAML::Node legacyMaterialNode(YAML::NodeType::Map);
	legacyMaterialNode["ShaderID"] = 3;
	MaterialComponent legacyMaterial;
	legacyMaterial.materials = materials;
	assert(MaterialComponentOperations::Decode(
		legacyMaterial,
		legacyMaterialNode
	));
	assert(legacyMaterial.ShaderID == 3);
	assert(legacyMaterial.materials.empty());

	// 同様に旧ModelRenderer YAMLにはSubMeshesが存在しない。
	// 空Overrideとして復元し、YAML::InvalidNodeを送出しない。
	YAML::Node legacyRendererNode(YAML::NodeType::Map);
	std::vector<ModelSubMeshRenderState> legacyStates;
	legacyStates.push_back(state);
	ModelMaterialYamlSerialization::DecodeSubMeshStates(
		ModelRendererSerialization::OptionalChild(
			legacyRendererNode,
			"SubMeshes"
		),
		legacyStates
	);
	assert(legacyStates.empty());

	return 0;
}
