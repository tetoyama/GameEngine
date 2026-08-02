#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string ReadText(const char* path){
	std::ifstream stream(path, std::ios::binary);
	assert(stream.is_open());
	std::ostringstream output;
	output << stream.rdbuf();
	return output.str();
}

void Require(const std::string& text, const char* token){
	assert(text.find(token) != std::string::npos);
}

} // namespace

int main(){
	const std::string materialComponent = ReadText(
		"Source/GameApplication/Engine/Scene/Component/materialComponent.h"
	);
	Require(materialComponent, "AllocateMaterialID");
	Require(materialComponent, "AddMaterial");
	Require(materialComponent, "RemoveMaterial");
	Require(materialComponent, "SanitizeMaterials");
	Require(materialComponent, "CustomMaterialCollection::Add");

	const std::string materialOperations = ReadText(
		"Source/GameApplication/Engine/Scene/Component/Operations/MaterialComponentOperations.h"
	);
	Require(materialOperations, "MaterialSchemaVersion");
	Require(materialOperations, "EncodeCustomMaterials");
	Require(materialOperations, "DecodeCustomMaterials");
	Require(materialOperations, "component.materials");
	Require(materialOperations, "MaterialDescriptorInspector::DrawCustomMaterials");
	Require(materialOperations, "OptionalChild");
	Require(materialOperations, "OptionalChild(node, \"Materials\")");
	Require(materialOperations, "catch(const YAML::Exception&)");

	const std::string descriptorInspector = ReadText(
		"Source/GameApplication/Engine/Scene/Component/Operations/MaterialDescriptorInspector.h"
	);
	Require(descriptorInspector, "Add Custom Material");
	Require(descriptorInspector, "Remove Custom Material");
	Require(descriptorInspector, "DrawRenderState");
	Require(descriptorInspector, "DrawTextures");
	Require(descriptorInspector, "Alpha Mode");
	Require(descriptorInspector, "UV Channel");

	const std::string rendererSerialization = ReadText(
		"Source/GameApplication/Engine/Scene/Component/Operations/ModelRendererSerialization.h"
	);
	Require(rendererSerialization, "SubMeshStateSchemaVersion");
	Require(rendererSerialization, "EncodeSubMeshStates");
	Require(rendererSerialization, "DecodeSubMeshStates");
	Require(rendererSerialization, "component.subMeshes");
	Require(rendererSerialization, "OptionalChild");
	Require(rendererSerialization, "OptionalChild(node, \"SubMeshes\")");
	Require(rendererSerialization, "TryDecodeValue");

	const std::string runtime = ReadText(
		"Source/GameApplication/Engine/Scene/Component/Operations/ModelRendererRuntime.h"
	);
	Require(runtime, "ModelSubMeshStateSynchronization::Synchronize");
	Require(runtime, "component.model->SubMeshes");
	Require(runtime, "SubMesh Overrideを保持する");

	const std::string pathInspector = ReadText(
		"Source/GameApplication/Engine/Scene/Component/Operations/ModelRendererInspectorCommon.h"
	);
	Require(pathInspector, "ChangeModelPath");
	Require(pathInspector, "component.subMeshes.clear();");
	Require(pathInspector, "ReloadCoordinateMode");

	const std::string subMeshInspector = ReadText(
		"Source/GameApplication/Engine/Scene/Component/Operations/ModelRendererSubMeshInspector.h"
	);
	Require(subMeshInspector, "EnsureState");
	Require(subMeshInspector, "Model Default");
	Require(subMeshInspector, "CustomMaterialDisplayName");
	Require(subMeshInspector, "DrawCustomMaterialAssignment");
	Require(subMeshInspector, "Missing ID");
	Require(subMeshInspector, "Reset SubMesh State");
	Require(subMeshInspector, "state.visible");
	Require(subMeshInspector, "state.castShadow");

	const std::string rendererInspector = ReadText(
		"Source/GameApplication/Engine/Scene/Component/Operations/ModelRendererInspectorMotion.h"
	);
	Require(rendererInspector, "ModelRendererSubMeshInspector::Draw");
	Require(rendererInspector, "component.subMeshes");
	Require(rendererInspector, "FindSiblingMaterialComponent");
	Require(rendererInspector, "GetAllBaseComponents<ModelRendererComponent>");
	Require(rendererInspector, "GetComponent<MaterialComponent>");

	const std::string yaml = ReadText(
		"Source/GameApplication/Engine/Resources/Data/modelMaterialYamlSerialization.h"
	);
	Require(yaml, "SchemaVersion = 1");
	Require(yaml, "MaterialAlphaMode");
	Require(yaml, "MaterialTextureBinding");
	Require(yaml, "CustomMaterialID");
	Require(yaml, "std::unordered_set<ModelSubMeshID>");
	return 0;
}
