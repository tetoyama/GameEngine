#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Engine/Scene/Component/materialComponent.h"
#include "Engine/Resources/Data/modelMaterialYamlSerialization.h"

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
	// Play開始前に保存された旧Temp SceneにはMaterialsが存在しない。
	// 欠落const NodeをInvalidNodeのままDecoderへ渡さず、空Collectionとして
	// 復元できることを実行時に確認する。
	std::vector<CustomMaterialEntry> existingMaterials;
	CustomMaterialEntry* existingMaterial =
		CustomMaterialCollection::Add(existingMaterials, "Legacy");
	assert(existingMaterial);

	YAML::Node legacyMaterialNode(YAML::NodeType::Map);
	legacyMaterialNode["ShaderID"] = 3;
	MaterialComponent legacyMaterial;
	legacyMaterial.materials = existingMaterials;
	assert(MaterialComponentOperations::Decode(
		legacyMaterial,
		legacyMaterialNode
	));
	assert(legacyMaterial.ShaderID == 3);
	assert(legacyMaterial.materials.empty());

	// 同様に旧ModelRenderer YAMLにはSubMeshesが存在しない。
	// Serializerと同じOptional Child契約を通して空Overrideへ復元し、
	// YAML::InvalidNodeを送出しない。
	YAML::Node legacyRendererNode(YAML::NodeType::Map);
	std::vector<ModelSubMeshRenderState> legacyStates;
	ModelSubMeshRenderState state;
	state.subMeshID = 17;
	legacyStates.push_back(state);
	ModelMaterialYamlSerialization::DecodeSubMeshStates(
		MaterialComponentOperations::OptionalChild(
			legacyRendererNode,
			"SubMeshes"
		),
		legacyStates
	);
	assert(legacyStates.empty());

	const std::string sceneManager = ReadText(
		"Source/GameApplication/Engine/Scene/sceneManager.cpp"
	);
	const std::size_t tempLoadBegin = sceneManager.find(
		"void SceneManager::TempLoad()"
	);
	assert(tempLoadBegin != std::string::npos);
	const std::string tempLoad = sceneManager.substr(tempLoadBegin);

	Require(tempLoad, "stagedScenes");
	Require(tempLoad, "std::filesystem::exists(tempScenePath)");
	Require(tempLoad, "YAML::LoadFile(tempScenePath)");
	Require(tempLoad, "m_activeScenes.swap(stagedScenes)");
	Require(tempLoad, "oldScene->Shutdown()");
	Require(tempLoad, "catch(const YAML::Exception& exception)");
	Require(tempLoad, "catch(const std::exception& exception)");

	const std::size_t stagePosition = tempLoad.find(
		"stagedScenes.emplace"
	);
	const std::size_t swapPosition = tempLoad.find(
		"m_activeScenes.swap(stagedScenes)"
	);
	const std::size_t oldShutdownPosition = tempLoad.find(
		"oldScene->Shutdown()"
	);
	assert(stagePosition != std::string::npos);
	assert(swapPosition != std::string::npos);
	assert(oldShutdownPosition != std::string::npos);
	assert(stagePosition < swapPosition);
	assert(swapPosition < oldShutdownPosition);

	// 復元失敗前にactive Sceneを破棄する旧実装へ戻さない。
	assert(tempLoad.find("m_activeScenes.clear();") == std::string::npos);

	const std::string sceneHeader = ReadText(
		"Source/GameApplication/Engine/Scene/scene.h"
	);
	Require(sceneHeader, "struct LifecycleGuard");
	Require(sceneHeader, "~LifecycleGuard() noexcept");
	Require(sceneHeader, "owner->Shutdown()");
	Require(sceneHeader, "LifecycleGuard m_lifecycleGuard{this}");

	const std::string materialOperations = ReadText(
		"Source/GameApplication/Engine/Scene/Component/Operations/MaterialComponentOperations.h"
	);
	Require(materialOperations, "OptionalChild(node, \"Materials\")");

	const std::string rendererSerialization = ReadText(
		"Source/GameApplication/Engine/Scene/Component/Operations/ModelRendererSerialization.h"
	);
	Require(rendererSerialization, "OptionalChild(node, \"SubMeshes\")");

	return 0;
}
