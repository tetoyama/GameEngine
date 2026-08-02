#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <backends/yaml-cpp/yaml.h>

#include "modelMaterialTypes.h"

namespace ModelMaterialYamlSerialization {

inline constexpr std::uint32_t SchemaVersion = 1;

template<class T>
inline bool TryReadValue(const YAML::Node& node, T& output){
	if(!node){
		return false;
	}
	try{
		output = node.as<T>();
		return true;
	}catch(const YAML::Exception&){
		return false;
	}
}

template<class T>
inline bool TryRead(
	const YAML::Node& node,
	const char* key,
	T& output
){
	return node.IsMap() && TryReadValue(node[key], output);
}

template<std::size_t Size>
inline YAML::Node EncodeArray(const std::array<float, Size>& values){
	YAML::Node node(YAML::NodeType::Sequence);
	for(float value : values){
		node.push_back(value);
	}
	return node;
}

template<std::size_t Size>
inline bool DecodeArray(
	const YAML::Node& node,
	std::array<float, Size>& values
){
	if(!node.IsSequence() || node.size() != Size){
		return false;
	}
	std::array<float, Size> decoded{};
	for(std::size_t index = 0; index < Size; ++index){
		if(!TryReadValue(node[index], decoded[index])){
			return false;
		}
	}
	values = decoded;
	return true;
}

inline const char* ToString(MaterialTextureSemantic value) noexcept {
	switch(value){
		case MaterialTextureSemantic::BaseColor: return "BaseColor";
		case MaterialTextureSemantic::Normal: return "Normal";
		case MaterialTextureSemantic::Bump: return "Bump";
		case MaterialTextureSemantic::Height: return "Height";
		case MaterialTextureSemantic::Metallic: return "Metallic";
		case MaterialTextureSemantic::Roughness: return "Roughness";
		case MaterialTextureSemantic::AmbientOcclusion: return "AmbientOcclusion";
		case MaterialTextureSemantic::Emissive: return "Emissive";
		case MaterialTextureSemantic::Opacity: return "Opacity";
	}
	return "BaseColor";
}

inline const char* ToString(MaterialColorSpace value) noexcept {
	return value == MaterialColorSpace::Linear ? "Linear" : "SRGB";
}

inline const char* ToString(MaterialAlphaMode value) noexcept {
	switch(value){
		case MaterialAlphaMode::Masked: return "Masked";
		case MaterialAlphaMode::Blend: return "Blend";
		case MaterialAlphaMode::Opaque:
		default: return "Opaque";
	}
}

inline const char* ToString(MaterialCullMode value) noexcept {
	switch(value){
		case MaterialCullMode::None: return "None";
		case MaterialCullMode::Front: return "Front";
		case MaterialCullMode::Back:
		default: return "Back";
	}
}

inline const char* ToString(SubMeshMaterialSource value) noexcept {
	return value == SubMeshMaterialSource::CustomMaterial
		? "CustomMaterial"
		: "ModelDefault";
}

inline bool Parse(
	const YAML::Node& node,
	MaterialTextureSemantic& output
){
	std::string value;
	if(!TryReadValue(node, value)) return false;
	if(value == "BaseColor") output = MaterialTextureSemantic::BaseColor;
	else if(value == "Normal") output = MaterialTextureSemantic::Normal;
	else if(value == "Bump") output = MaterialTextureSemantic::Bump;
	else if(value == "Height") output = MaterialTextureSemantic::Height;
	else if(value == "Metallic") output = MaterialTextureSemantic::Metallic;
	else if(value == "Roughness") output = MaterialTextureSemantic::Roughness;
	else if(value == "AmbientOcclusion") output = MaterialTextureSemantic::AmbientOcclusion;
	else if(value == "Emissive") output = MaterialTextureSemantic::Emissive;
	else if(value == "Opacity") output = MaterialTextureSemantic::Opacity;
	else return false;
	return true;
}

inline bool Parse(
	const YAML::Node& node,
	MaterialColorSpace& output
){
	std::string value;
	if(!TryReadValue(node, value)) return false;
	if(value == "Linear") output = MaterialColorSpace::Linear;
	else if(value == "SRGB") output = MaterialColorSpace::SRGB;
	else return false;
	return true;
}

inline bool Parse(
	const YAML::Node& node,
	MaterialAlphaMode& output
){
	std::string value;
	if(!TryReadValue(node, value)) return false;
	if(value == "Opaque") output = MaterialAlphaMode::Opaque;
	else if(value == "Masked") output = MaterialAlphaMode::Masked;
	else if(value == "Blend") output = MaterialAlphaMode::Blend;
	else return false;
	return true;
}

inline bool Parse(
	const YAML::Node& node,
	MaterialCullMode& output
){
	std::string value;
	if(!TryReadValue(node, value)) return false;
	if(value == "None") output = MaterialCullMode::None;
	else if(value == "Front") output = MaterialCullMode::Front;
	else if(value == "Back") output = MaterialCullMode::Back;
	else return false;
	return true;
}

inline bool Parse(
	const YAML::Node& node,
	SubMeshMaterialSource& output
){
	std::string value;
	if(!TryReadValue(node, value)) return false;
	if(value == "ModelDefault") output = SubMeshMaterialSource::ModelDefault;
	else if(value == "CustomMaterial") output = SubMeshMaterialSource::CustomMaterial;
	else return false;
	return true;
}

inline float FiniteOr(float value, float fallback) noexcept {
	return std::isfinite(value) ? value : fallback;
}

inline void Sanitize(MaterialDescriptor& descriptor) noexcept {
	descriptor.shaderID = (std::max)(descriptor.shaderID, 0);
	for(std::size_t index = 0; index < 3; ++index){
		descriptor.parameters.baseColor[index] = FiniteOr(
			descriptor.parameters.baseColor[index],
			1.0f
		);
		descriptor.parameters.emissiveColor[index] = FiniteOr(
			descriptor.parameters.emissiveColor[index],
			0.0f
		);
	}
	descriptor.parameters.baseColor[3] = (std::clamp)(
		FiniteOr(descriptor.parameters.baseColor[3], 1.0f),
		0.0f,
		1.0f
	);
	descriptor.parameters.metallic = (std::clamp)(
		FiniteOr(descriptor.parameters.metallic, 0.0f), 0.0f, 1.0f
	);
	descriptor.parameters.roughness = (std::clamp)(
		FiniteOr(descriptor.parameters.roughness, 1.0f), 0.0f, 1.0f
	);
	descriptor.parameters.ambientOcclusion = (std::clamp)(
		FiniteOr(descriptor.parameters.ambientOcclusion, 1.0f), 0.0f, 1.0f
	);
	descriptor.parameters.emissiveIntensity = (std::max)(
		FiniteOr(descriptor.parameters.emissiveIntensity, 0.0f), 0.0f
	);
	descriptor.parameters.opacity = (std::clamp)(
		FiniteOr(descriptor.parameters.opacity, 1.0f), 0.0f, 1.0f
	);
	descriptor.parameters.normalScale = (std::max)(
		FiniteOr(descriptor.parameters.normalScale, 1.0f), 0.0f
	);
	descriptor.parameters.heightScale = FiniteOr(
		descriptor.parameters.heightScale,
		0.0f
	);
	descriptor.renderState.alphaCutoff = (std::clamp)(
		FiniteOr(descriptor.renderState.alphaCutoff, 0.5f),
		0.0f,
		1.0f
	);
	for(MaterialTextureBinding& texture : descriptor.textures){
		for(float& value : texture.uvScale) value = FiniteOr(value, 1.0f);
		for(float& value : texture.uvOffset) value = FiniteOr(value, 0.0f);
		texture.uvRotation = FiniteOr(texture.uvRotation, 0.0f);
		texture.strength = (std::max)(FiniteOr(texture.strength, 1.0f), 0.0f);
	}
}

inline YAML::Node EncodeTexture(const MaterialTextureBinding& texture){
	YAML::Node node;
	node["Semantic"] = ToString(texture.semantic);
	node["ColorSpace"] = ToString(texture.colorSpace);
	if(!texture.sourcePath.empty()) node["SourcePath"] = texture.sourcePath;
	if(!texture.assetPath.empty()) node["AssetPath"] = texture.assetPath;
	if(texture.sourceTextureIndex != InvalidModelSourceIndex){
		node["SourceTextureIndex"] = texture.sourceTextureIndex;
	}
	node["UVChannel"] = static_cast<std::uint32_t>(texture.uvChannel);
	node["UVScale"] = EncodeArray(texture.uvScale);
	node["UVOffset"] = EncodeArray(texture.uvOffset);
	node["UVRotation"] = texture.uvRotation;
	node["Strength"] = texture.strength;
	node["Embedded"] = texture.embedded;
	return node;
}

inline bool DecodeTexture(
	const YAML::Node& node,
	MaterialTextureBinding& texture
){
	if(!node.IsMap()) return false;
	MaterialTextureBinding decoded;
	if(!Parse(node["Semantic"], decoded.semantic)) return false;
	decoded.colorSpace =
		decoded.semantic == MaterialTextureSemantic::BaseColor ||
		decoded.semantic == MaterialTextureSemantic::Emissive
			? MaterialColorSpace::SRGB
			: MaterialColorSpace::Linear;
	Parse(node["ColorSpace"], decoded.colorSpace);
	TryRead(node, "SourcePath", decoded.sourcePath);
	TryRead(node, "AssetPath", decoded.assetPath);
	TryRead(node, "SourceTextureIndex", decoded.sourceTextureIndex);
	std::uint32_t uvChannel = decoded.uvChannel;
	if(TryRead(node, "UVChannel", uvChannel)){
		decoded.uvChannel = static_cast<std::uint8_t>(
			(std::min)(uvChannel, std::uint32_t{255})
		);
	}
	DecodeArray(node["UVScale"], decoded.uvScale);
	DecodeArray(node["UVOffset"], decoded.uvOffset);
	TryRead(node, "UVRotation", decoded.uvRotation);
	TryRead(node, "Strength", decoded.strength);
	TryRead(node, "Embedded", decoded.embedded);
	if(decoded.sourcePath.empty() && decoded.assetPath.empty() &&
		decoded.sourceTextureIndex == InvalidModelSourceIndex){
		return false;
	}
	texture = std::move(decoded);
	return true;
}

inline YAML::Node EncodeDescriptor(const MaterialDescriptor& descriptor){
	YAML::Node node;
	node["Version"] = SchemaVersion;
	node["ShaderID"] = descriptor.shaderID;

	YAML::Node parameters;
	parameters["BaseColor"] = EncodeArray(descriptor.parameters.baseColor);
	parameters["EmissiveColor"] = EncodeArray(descriptor.parameters.emissiveColor);
	parameters["Metallic"] = descriptor.parameters.metallic;
	parameters["Roughness"] = descriptor.parameters.roughness;
	parameters["AmbientOcclusion"] = descriptor.parameters.ambientOcclusion;
	parameters["EmissiveIntensity"] = descriptor.parameters.emissiveIntensity;
	parameters["Opacity"] = descriptor.parameters.opacity;
	parameters["NormalScale"] = descriptor.parameters.normalScale;
	parameters["HeightScale"] = descriptor.parameters.heightScale;
	node["Parameters"] = parameters;

	if(!descriptor.textures.empty()){
		YAML::Node textures(YAML::NodeType::Sequence);
		for(const MaterialTextureBinding& texture : descriptor.textures){
			textures.push_back(EncodeTexture(texture));
		}
		node["Textures"] = textures;
	}

	YAML::Node renderState;
	renderState["AlphaMode"] = ToString(descriptor.renderState.alphaMode);
	renderState["CullMode"] = ToString(descriptor.renderState.cullMode);
	renderState["AlphaCutoff"] = descriptor.renderState.alphaCutoff;
	renderState["DepthWrite"] = descriptor.renderState.depthWrite;
	renderState["ReceiveShadow"] = descriptor.renderState.receiveShadow;
	node["RenderState"] = renderState;
	node["LegacyMaterialFlags"] = descriptor.legacyMaterialFlags;
	return node;
}

inline bool DecodeDescriptor(
	const YAML::Node& node,
	MaterialDescriptor& descriptor
){
	if(!node.IsMap()) return false;
	MaterialDescriptor decoded;
	TryRead(node, "ShaderID", decoded.shaderID);

	const YAML::Node parameters = node["Parameters"];
	if(parameters.IsMap()){
		DecodeArray(parameters["BaseColor"], decoded.parameters.baseColor);
		DecodeArray(parameters["EmissiveColor"], decoded.parameters.emissiveColor);
		TryRead(parameters, "Metallic", decoded.parameters.metallic);
		TryRead(parameters, "Roughness", decoded.parameters.roughness);
		TryRead(parameters, "AmbientOcclusion", decoded.parameters.ambientOcclusion);
		TryRead(parameters, "EmissiveIntensity", decoded.parameters.emissiveIntensity);
		TryRead(parameters, "Opacity", decoded.parameters.opacity);
		TryRead(parameters, "NormalScale", decoded.parameters.normalScale);
		TryRead(parameters, "HeightScale", decoded.parameters.heightScale);
	}

	const YAML::Node textures = node["Textures"];
	if(textures.IsSequence()){
		decoded.textures.reserve(textures.size());
		for(const auto& textureNode : textures){
			MaterialTextureBinding texture;
			if(DecodeTexture(textureNode, texture)){
				decoded.textures.push_back(std::move(texture));
			}
		}
	}

	const YAML::Node renderState = node["RenderState"];
	if(renderState.IsMap()){
		Parse(renderState["AlphaMode"], decoded.renderState.alphaMode);
		Parse(renderState["CullMode"], decoded.renderState.cullMode);
		TryRead(renderState, "AlphaCutoff", decoded.renderState.alphaCutoff);
		TryRead(renderState, "DepthWrite", decoded.renderState.depthWrite);
		TryRead(renderState, "ReceiveShadow", decoded.renderState.receiveShadow);
	}
	TryRead(node, "LegacyMaterialFlags", decoded.legacyMaterialFlags);
	Sanitize(decoded);
	descriptor = std::move(decoded);
	return true;
}

inline YAML::Node EncodeCustomMaterials(
	const std::vector<CustomMaterialEntry>& materials
){
	YAML::Node node(YAML::NodeType::Sequence);
	for(const CustomMaterialEntry& material : materials){
		if(material.id == InvalidCustomMaterialID) continue;
		YAML::Node entry;
		entry["ID"] = material.id;
		if(!material.name.empty()) entry["Name"] = material.name;
		entry["Descriptor"] = EncodeDescriptor(material.inlineMaterial);
		node.push_back(entry);
	}
	return node;
}

inline void DecodeCustomMaterials(
	const YAML::Node& node,
	std::vector<CustomMaterialEntry>& materials
){
	materials.clear();
	if(!node.IsSequence()) return;
	std::unordered_set<CustomMaterialID> ids;
	ids.reserve(node.size());
	for(const auto& entryNode : node){
		if(!entryNode.IsMap()) continue;
		CustomMaterialEntry entry;
		if(!TryRead(entryNode, "ID", entry.id) ||
			entry.id == InvalidCustomMaterialID ||
			!ids.insert(entry.id).second){
			continue;
		}
		TryRead(entryNode, "Name", entry.name);
		DecodeDescriptor(entryNode["Descriptor"], entry.inlineMaterial);
		materials.push_back(std::move(entry));
	}
}

inline bool IsDefaultSubMeshState(
	const ModelSubMeshRenderState& state
) noexcept {
	return state.visible && state.castShadow &&
		state.material.source == SubMeshMaterialSource::ModelDefault &&
		state.material.customMaterialID == InvalidCustomMaterialID;
}

inline YAML::Node EncodeSubMeshStates(
	const std::vector<ModelSubMeshRenderState>& states
){
	YAML::Node node(YAML::NodeType::Sequence);
	for(const ModelSubMeshRenderState& state : states){
		if(state.subMeshID == InvalidModelSubMeshID ||
			IsDefaultSubMeshState(state)){
			continue;
		}
		YAML::Node entry;
		entry["SubMeshID"] = state.subMeshID;
		entry["Visible"] = state.visible;
		entry["CastShadow"] = state.castShadow;
		entry["MaterialSource"] = ToString(state.material.source);
		if(state.material.source == SubMeshMaterialSource::CustomMaterial &&
			state.material.customMaterialID != InvalidCustomMaterialID){
			entry["CustomMaterialID"] = state.material.customMaterialID;
		}
		node.push_back(entry);
	}
	return node;
}

inline void DecodeSubMeshStates(
	const YAML::Node& node,
	std::vector<ModelSubMeshRenderState>& states
){
	states.clear();
	if(!node.IsSequence()) return;
	std::unordered_set<ModelSubMeshID> ids;
	ids.reserve(node.size());
	for(const auto& entryNode : node){
		if(!entryNode.IsMap()) continue;
		ModelSubMeshRenderState state;
		if(!TryRead(entryNode, "SubMeshID", state.subMeshID) ||
			state.subMeshID == InvalidModelSubMeshID ||
			!ids.insert(state.subMeshID).second){
			continue;
		}
		TryRead(entryNode, "Visible", state.visible);
		TryRead(entryNode, "CastShadow", state.castShadow);
		Parse(entryNode["MaterialSource"], state.material.source);
		if(state.material.source == SubMeshMaterialSource::CustomMaterial){
			TryRead(
				entryNode,
				"CustomMaterialID",
				state.material.customMaterialID
			);
			if(state.material.customMaterialID == InvalidCustomMaterialID){
				state.material.source = SubMeshMaterialSource::ModelDefault;
			}
		}else{
			state.material.customMaterialID = InvalidCustomMaterialID;
		}
		states.push_back(std::move(state));
	}
}

} // namespace ModelMaterialYamlSerialization
