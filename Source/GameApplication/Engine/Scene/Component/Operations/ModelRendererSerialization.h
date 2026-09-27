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

inline YAML::Node OptionalChild(
	const YAML::Node& node,
	const char* key
) noexcept {
	try{
		if(!node.IsMap()) return YAML::Node{};
		const YAML::Node child = node[key];
		if(child.IsDefined()) return child;
	}catch(const YAML::Exception&){
	}
	return YAML::Node{};
}

template<class T>
inline bool TryDecodeValue(
	const YAML::Node& node,
	const char* key,
	T& output
) noexcept {
	try{
		if(!node.IsMap()) return false;
		const YAML::Node child = node[key];
		if(!child.IsDefined() || child.IsNull()) return false;
		output = child.as<T>();
		return true;
	}catch(const YAML::Exception&){
		return false;
	}
}

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
	try{
		if(!node.IsMap()){
			return false;
		}
	}catch(const YAML::Exception&){
		return false;
	}

	TryDecodeValue(node, "FilePath", component.modelFilePath);
	TryDecodeValue(node, "isBlender", component.isBlender);
	TryDecodeValue(node, "AnimationTime", component.animationTime);

	component.animations.clear();
	const YAML::Node animations = OptionalChild(node, "Animations");
	try{
		if(animations.IsMap()){
			for(const auto& animationNode : animations){
				try{
					component.animations.emplace_back(
						animationNode.first.as<std::string>(),
						animationNode.second.as<std::string>()
					);
				}catch(const YAML::Exception&){
					// 壊れたAnimation Entryだけを無視する。
				}
			}
		}
	}catch(const YAML::Exception&){
		component.animations.clear();
	}

	// 旧Scene / Play開始前のTemp SceneにはSubMeshes Nodeが存在しない。
	// 欠落Keyを安全なNull Nodeへ変換し、空Overrideとして復元する。
	ModelMaterialYamlSerialization::DecodeSubMeshStates(
		OptionalChild(node, "SubMeshes"),
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
