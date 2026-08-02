// =======================================================================
//
// ModelRendererSerialization.h
//
// ModelRendererComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include "Backends/YAMLConverters.h"
#include "Resources/Data/modelMaterialYamlSerialization.h"

namespace ModelRendererSerialization {

inline YAML::Node Encode(const ModelRendererComponent& component){
	YAML::Node node;
	const std::string filePath = component.model
		? component.model->FilePath
		: component.modelFilePath;
	if(!filePath.empty()){
		node["FilePath"] = filePath;
	}
	node["isBlender"] = component.isBlender;
	node["AnimationTime"] = component.animationTime;
	for(const auto& [animationName, animationPath] : component.animations){
		node["Animations"][animationName] = animationPath;
	}

	const YAML::Node subMeshStates =
		ModelMaterialYamlSerialization::EncodeSubMeshStates(
			component.subMeshes
		);
	if(subMeshStates.size() != 0){
		node["SubMeshStateSchemaVersion"] =
			ModelMaterialYamlSerialization::SchemaVersion;
		node["SubMeshes"] = subMeshStates;
	}
	return node;
}

inline bool Decode(
	ModelRendererComponent& component,
	SceneContext* context,
	const YAML::Node& node
){
	if(!node.IsMap()){
		return false;
	}

	if(node["FilePath"]){
		component.modelFilePath = node["FilePath"].as<std::string>();
	}
	if(node["isBlender"]){
		component.isBlender = node["isBlender"].as<bool>();
	}
	if(node["AnimationTime"]){
		component.animationTime = node["AnimationTime"].as<float>();
	}

	component.animations.clear();
	if(node["Animations"] && node["Animations"].IsMap()){
		for(const auto& animationNode : node["Animations"]){
			component.animations.emplace_back(
				animationNode.first.as<std::string>(),
				animationNode.second.as<std::string>()
			);
		}
	}

	ModelMaterialYamlSerialization::DecodeSubMeshStates(
		node["SubMeshes"],
		component.subMeshes
	);
	component.blendedAnimations.clear();
	component.CreateModel(context);
	return true;
}

} // namespace ModelRendererSerialization

inline YAML::Node ModelRendererComponent::encode(){
	return ModelRendererSerialization::Encode(*this);
}

inline bool ModelRendererComponent::decode(
	SceneContext* context,
	const YAML::Node& node
){
	return ModelRendererSerialization::Decode(*this, context, node);
}
