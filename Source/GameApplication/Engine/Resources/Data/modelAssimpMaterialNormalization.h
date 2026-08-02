#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "modelMaterialTypes.h"

namespace ModelAssimpMaterialNormalization {

inline std::string NormalizeTexturePath(std::string_view source){
	std::string normalized;
	normalized.reserve(source.size());
	bool previousSlash = false;
	for(char value : source){
		const char output = value == '\\' ? '/' : value;
		if(output == '/'){
			if(previousSlash) continue;
			previousSlash = true;
		}else{
			previousSlash = false;
		}
		normalized.push_back(output);
	}
	while(normalized.starts_with("./")){
		normalized.erase(0, 2);
	}
	return normalized;
}

inline std::uint32_t HashLocalIdentity(
	std::string_view domain,
	std::string_view identity
) noexcept {
	std::uint32_t hash = 2166136261u;
	auto append = [&hash](std::string_view text){
		for(char value : text){
			hash ^= static_cast<std::uint32_t>(
				static_cast<unsigned char>(value)
			);
			hash *= 16777619u;
		}
	};
	append(domain);
	hash ^= 0xffu;
	hash *= 16777619u;
	append(identity);
	return hash == 0 ? 1u : hash;
}

class StableLocalIDAllocator {
public:
	std::uint32_t Allocate(
		std::string_view domain,
		std::string_view identity,
		std::uint32_t sourceIndex,
		std::vector<ModelMaterialImportDiagnostic>* diagnostics = nullptr
	){
		std::string fallbackIdentity;
		if(identity.empty()){
			fallbackIdentity = "source-index:" + std::to_string(sourceIndex);
			identity = fallbackIdentity;
		}

		std::uint32_t candidate = HashLocalIdentity(domain, identity);
		std::uint32_t salt = sourceIndex + 1u;
		bool collision = false;
		while(candidate == 0 || m_used.contains(candidate)){
			collision = true;
			candidate ^= 0x9e3779b9u + salt +
				(candidate << 6u) + (candidate >> 2u);
			if(candidate == 0) candidate = 1;
			++salt;
		}
		m_used.insert(candidate);

		if(collision && diagnostics){
			diagnostics->push_back({
				ModelMaterialImportDiagnosticSeverity::Warning,
				ModelMaterialImportDiagnosticCode::StableLocalIDCollision,
				"Stable local ID collision was resolved with a deterministic salt.",
				sourceIndex,
				InvalidModelSourceIndex
			});
		}
		return candidate;
	}

private:
	std::unordered_set<std::uint32_t> m_used;
};

inline std::uint32_t ParseEmbeddedTextureIndex(
	std::string_view path
) noexcept {
	if(path.size() < 2 || path.front() != '*'){
		return InvalidModelSourceIndex;
	}
	std::uint32_t result = InvalidModelSourceIndex;
	const char* first = path.data() + 1;
	const char* last = path.data() + path.size();
	const auto parsed = std::from_chars(first, last, result);
	return parsed.ec == std::errc{} && parsed.ptr == last
		? result
		: InvalidModelSourceIndex;
}

inline MaterialColorSpace ColorSpaceFor(
	MaterialTextureSemantic semantic
) noexcept {
	return semantic == MaterialTextureSemantic::BaseColor ||
		semantic == MaterialTextureSemantic::Emissive
		? MaterialColorSpace::SRGB
		: MaterialColorSpace::Linear;
}

inline bool AppendFirstTexture(
	const aiMaterial& material,
	MaterialTextureSemantic semantic,
	std::initializer_list<aiTextureType> sourceTypes,
	std::uint32_t sourceMaterialIndex,
	std::vector<MaterialTextureBinding>& output,
	std::vector<ModelMaterialImportDiagnostic>& diagnostics
){
	for(aiTextureType sourceType : sourceTypes){
		if(material.GetTextureCount(sourceType) == 0) continue;

		aiString sourcePath;
		aiTextureMapping mapping = aiTextureMapping_UV;
		unsigned int uvIndex = 0;
		ai_real blend = static_cast<ai_real>(1.0);
		aiTextureMapMode mapModes[3] = {
			aiTextureMapMode_Wrap,
			aiTextureMapMode_Wrap,
			aiTextureMapMode_Wrap
		};
		if(material.GetTexture(
			sourceType,
			0,
			&sourcePath,
			&mapping,
			&uvIndex,
			&blend,
			nullptr,
			mapModes
		) != AI_SUCCESS){
			continue;
		}

		const std::string original = sourcePath.C_Str();
		const std::string normalized = NormalizeTexturePath(original);
		if(normalized.empty()){
			diagnostics.push_back({
				ModelMaterialImportDiagnosticSeverity::Warning,
				ModelMaterialImportDiagnosticCode::EmptyTexturePath,
				"Material texture reference has an empty path.",
				sourceMaterialIndex,
				InvalidModelSourceIndex
			});
			return false;
		}

		MaterialTextureBinding binding;
		binding.semantic = semantic;
		binding.colorSpace = ColorSpaceFor(semantic);
		binding.sourcePath = original;
		binding.assetPath = normalized;
		binding.embedded = normalized.front() == '*';
		binding.sourceTextureIndex = ParseEmbeddedTextureIndex(normalized);
		binding.uvChannel = static_cast<std::uint8_t>(
			(std::min)(uvIndex, 255u)
		);
		binding.strength = std::isfinite(static_cast<float>(blend))
			? static_cast<float>(blend)
			: 1.0f;

		aiUVTransform transform;
		if(material.Get(
			AI_MATKEY_UVTRANSFORM(sourceType, 0),
			transform
		) == AI_SUCCESS){
			binding.uvScale = {
				static_cast<float>(transform.mScaling.x),
				static_cast<float>(transform.mScaling.y)
			};
			binding.uvOffset = {
				static_cast<float>(transform.mTranslation.x),
				static_cast<float>(transform.mTranslation.y)
			};
			binding.uvRotation = static_cast<float>(transform.mRotation);
		}

		if(mapping != aiTextureMapping_UV){
			diagnostics.push_back({
				ModelMaterialImportDiagnosticSeverity::Warning,
				ModelMaterialImportDiagnosticCode::UnsupportedTextureMapping,
				"Non-UV texture mapping was preserved as a reference but requires conversion.",
				sourceMaterialIndex,
				InvalidModelSourceIndex
			});
		}

		output.push_back(std::move(binding));
		return true;
	}
	return false;
}

inline void CollectMeshNodePaths(
	const aiNode* node,
	std::string_view parentPath,
	std::vector<std::string>& meshNodePaths
){
	if(!node) return;
	std::string nodeName = node->mName.C_Str();
	if(nodeName.empty()) nodeName = "Node";
	std::string nodePath;
	if(parentPath.empty()){
		nodePath = nodeName;
	}else{
		nodePath.reserve(parentPath.size() + 1 + nodeName.size());
		nodePath.assign(parentPath);
		nodePath.push_back('/');
		nodePath.append(nodeName);
	}

	for(unsigned int index = 0; index < node->mNumMeshes; ++index){
		const unsigned int meshIndex = node->mMeshes[index];
		if(meshIndex < meshNodePaths.size() && meshNodePaths[meshIndex].empty()){
			meshNodePaths[meshIndex] = nodePath;
		}
	}
	for(unsigned int index = 0; index < node->mNumChildren; ++index){
		CollectMeshNodePaths(node->mChildren[index], nodePath, meshNodePaths);
	}
}

inline ImportedMaterialDefinition NormalizeMaterial(
	const aiMaterial& source,
	std::uint32_t sourceMaterialIndex,
	ImportedMaterialID id,
	std::vector<ModelMaterialImportDiagnostic>& diagnostics
){
	ImportedMaterialDefinition result;
	result.id = id;
	result.sourceMaterialIndex = sourceMaterialIndex;

	aiString materialName;
	if(source.Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS){
		result.name = materialName.C_Str();
	}
	if(result.name.empty()){
		result.name = "Material_" + std::to_string(sourceMaterialIndex);
		diagnostics.push_back({
			ModelMaterialImportDiagnosticSeverity::Info,
			ModelMaterialImportDiagnosticCode::MissingMaterialName,
			"Unnamed imported material received a deterministic fallback name.",
			sourceMaterialIndex,
			InvalidModelSourceIndex
		});
	}

	aiColor4D baseColor;
	if(source.Get(AI_MATKEY_BASE_COLOR, baseColor) != AI_SUCCESS){
		source.Get(AI_MATKEY_COLOR_DIFFUSE, baseColor);
	}
	result.descriptor.parameters.baseColor = {
		baseColor.r,
		baseColor.g,
		baseColor.b,
		baseColor.a
	};

	aiColor3D emissiveColor(0.0f, 0.0f, 0.0f);
	if(source.Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS){
		result.descriptor.parameters.emissiveColor = {
			emissiveColor.r,
			emissiveColor.g,
			emissiveColor.b
		};
	}

	ai_real metallic = static_cast<ai_real>(0.0);
	if(source.Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS){
		result.descriptor.parameters.metallic = std::clamp(
			static_cast<float>(metallic),
			0.0f,
			1.0f
		);
	}
	ai_real roughness = static_cast<ai_real>(1.0);
	if(source.Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS){
		result.descriptor.parameters.roughness = std::clamp(
			static_cast<float>(roughness),
			0.0f,
			1.0f
		);
	}
	ai_real opacity = static_cast<ai_real>(1.0);
	if(source.Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS){
		result.descriptor.parameters.opacity = std::clamp(
			static_cast<float>(opacity),
			0.0f,
			1.0f
		);
		result.descriptor.parameters.baseColor[3] *=
			result.descriptor.parameters.opacity;
	}
	ai_real emissiveIntensity = static_cast<ai_real>(0.0);
	if(source.Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveIntensity) == AI_SUCCESS){
		result.descriptor.parameters.emissiveIntensity =
			(std::max)(0.0f, static_cast<float>(emissiveIntensity));
	}else if(emissiveColor.r != 0.0f ||
		emissiveColor.g != 0.0f ||
		emissiveColor.b != 0.0f){
		result.descriptor.parameters.emissiveIntensity = 1.0f;
	}
	ai_real bumpScale = static_cast<ai_real>(0.0);
	if(source.Get(AI_MATKEY_BUMPSCALING, bumpScale) == AI_SUCCESS){
		result.descriptor.parameters.heightScale = static_cast<float>(bumpScale);
	}

	int twoSided = 0;
	if(source.Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS && twoSided != 0){
		result.descriptor.renderState.cullMode = MaterialCullMode::None;
	}

	AppendFirstTexture(
		source,
		MaterialTextureSemantic::BaseColor,
		{aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE},
		sourceMaterialIndex,
		result.descriptor.textures,
		diagnostics
	);
	AppendFirstTexture(
		source,
		MaterialTextureSemantic::Normal,
		{aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA},
		sourceMaterialIndex,
		result.descriptor.textures,
		diagnostics
	);
	AppendFirstTexture(
		source,
		MaterialTextureSemantic::Bump,
		{aiTextureType_HEIGHT},
		sourceMaterialIndex,
		result.descriptor.textures,
		diagnostics
	);
	AppendFirstTexture(
		source,
		MaterialTextureSemantic::Height,
		{aiTextureType_DISPLACEMENT},
		sourceMaterialIndex,
		result.descriptor.textures,
		diagnostics
	);
	AppendFirstTexture(
		source,
		MaterialTextureSemantic::Metallic,
		{aiTextureType_METALNESS},
		sourceMaterialIndex,
		result.descriptor.textures,
		diagnostics
	);
	AppendFirstTexture(
		source,
		MaterialTextureSemantic::Roughness,
		{aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_SHININESS},
		sourceMaterialIndex,
		result.descriptor.textures,
		diagnostics
	);
	AppendFirstTexture(
		source,
		MaterialTextureSemantic::AmbientOcclusion,
		{aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP},
		sourceMaterialIndex,
		result.descriptor.textures,
		diagnostics
	);
	AppendFirstTexture(
		source,
		MaterialTextureSemantic::Emissive,
		{aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE},
		sourceMaterialIndex,
		result.descriptor.textures,
		diagnostics
	);
	const bool hasOpacityTexture = AppendFirstTexture(
		source,
		MaterialTextureSemantic::Opacity,
		{aiTextureType_OPACITY},
		sourceMaterialIndex,
		result.descriptor.textures,
		diagnostics
	);
	if(hasOpacityTexture || result.descriptor.parameters.opacity < 0.999f ||
		result.descriptor.parameters.baseColor[3] < 0.999f){
		result.descriptor.renderState.alphaMode = MaterialAlphaMode::Blend;
		result.descriptor.renderState.depthWrite = false;
	}
	return result;
}

inline bool Normalize(
	const aiScene* scene,
	std::vector<ImportedMaterialDefinition>& materials,
	std::vector<ModelSubMeshDefinition>& subMeshes,
	std::vector<ModelMaterialImportDiagnostic>& diagnostics
){
	if(!scene) return false;

	materials.clear();
	subMeshes.clear();
	diagnostics.clear();
	materials.reserve(scene->mNumMaterials);
	subMeshes.reserve(scene->mNumMeshes);

	StableLocalIDAllocator materialIDs;
	std::vector<ImportedMaterialID> materialIDBySourceIndex(
		scene->mNumMaterials,
		InvalidImportedMaterialID
	);
	for(unsigned int index = 0; index < scene->mNumMaterials; ++index){
		const aiMaterial* source = scene->mMaterials
			? scene->mMaterials[index]
			: nullptr;
		if(!source) continue;
		std::string identity = source->GetName().C_Str();
		const ImportedMaterialID id = materialIDs.Allocate(
			"material",
			identity,
			index,
			&diagnostics
		);
		materialIDBySourceIndex[index] = id;
		materials.push_back(NormalizeMaterial(
			*source,
			index,
			id,
			diagnostics
		));
	}

	std::vector<std::string> meshNodePaths(scene->mNumMeshes);
	CollectMeshNodePaths(scene->mRootNode, {}, meshNodePaths);
	StableLocalIDAllocator subMeshIDs;
	for(unsigned int index = 0; index < scene->mNumMeshes; ++index){
		const aiMesh* mesh = scene->mMeshes ? scene->mMeshes[index] : nullptr;
		if(!mesh) continue;

		ModelSubMeshDefinition definition;
		definition.name = mesh->mName.C_Str();
		if(definition.name.empty()){
			definition.name = "Mesh_" + std::to_string(index);
			diagnostics.push_back({
				ModelMaterialImportDiagnosticSeverity::Info,
				ModelMaterialImportDiagnosticCode::MissingMeshName,
				"Unnamed mesh received a deterministic fallback name.",
				InvalidModelSourceIndex,
				index
			});
		}
		definition.nodePath = index < meshNodePaths.size()
			? meshNodePaths[index]
			: std::string{};
		definition.geometryIndex = index;
		std::string identity = definition.nodePath;
		if(!identity.empty()) identity.push_back('/');
		identity.append(definition.name);
		definition.id = subMeshIDs.Allocate(
			"submesh",
			identity,
			index,
			&diagnostics
		);
		if(mesh->mMaterialIndex < materialIDBySourceIndex.size()){
			definition.defaultMaterialID =
				materialIDBySourceIndex[mesh->mMaterialIndex];
		}else{
			diagnostics.push_back({
				ModelMaterialImportDiagnosticSeverity::Warning,
				ModelMaterialImportDiagnosticCode::InvalidMaterialIndex,
				"Mesh references a material index outside the imported material table.",
				mesh->mMaterialIndex,
				index
			});
		}
		subMeshes.push_back(std::move(definition));
	}
	return true;
}

} // namespace ModelAssimpMaterialNormalization
