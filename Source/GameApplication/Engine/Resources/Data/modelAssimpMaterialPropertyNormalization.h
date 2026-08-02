#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "modelMaterialTypes.h"

// This adapter intentionally reads aiMaterialProperty storage directly.
// Render extraction smoke tests include ModelData without linking Assimp's
// implementation library, so normalization must not call aiMaterial methods.
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

inline const aiMaterialProperty* FindProperty(
	const aiMaterial& material,
	const char* key,
	unsigned int semantic,
	unsigned int index
) noexcept {
	if(!key || !material.mProperties) return nullptr;
	for(unsigned int propertyIndex = 0;
		propertyIndex < material.mNumProperties;
		++propertyIndex){
		const aiMaterialProperty* property =
			material.mProperties[propertyIndex];
		if(!property || property->mSemantic != semantic ||
			property->mIndex != index){
			continue;
		}
		if(std::string_view(property->mKey.C_Str()) == key){
			return property;
		}
	}
	return nullptr;
}

inline bool ReadRealProperty(
	const aiMaterial& material,
	const char* key,
	unsigned int semantic,
	unsigned int index,
	float& output
) noexcept {
	const aiMaterialProperty* property =
		FindProperty(material, key, semantic, index);
	if(!property || !property->mData) return false;
	if(property->mType == aiPTI_Float &&
		property->mDataLength >= sizeof(ai_real)){
		ai_real value{};
		std::memcpy(&value, property->mData, sizeof(value));
		output = static_cast<float>(value);
		return std::isfinite(output);
	}
	if(property->mType == aiPTI_Double &&
		property->mDataLength >= sizeof(double)){
		double value{};
		std::memcpy(&value, property->mData, sizeof(value));
		output = static_cast<float>(value);
		return std::isfinite(output);
	}
	if(property->mType == aiPTI_Integer &&
		property->mDataLength >= sizeof(std::int32_t)){
		std::int32_t value{};
		std::memcpy(&value, property->mData, sizeof(value));
		output = static_cast<float>(value);
		return true;
	}
	return false;
}

inline bool ReadIntegerProperty(
	const aiMaterial& material,
	const char* key,
	unsigned int semantic,
	unsigned int index,
	std::int32_t& output
) noexcept {
	const aiMaterialProperty* property =
		FindProperty(material, key, semantic, index);
	if(!property || !property->mData) return false;
	if(property->mType == aiPTI_Integer &&
		property->mDataLength >= sizeof(std::int32_t)){
		std::memcpy(&output, property->mData, sizeof(output));
		return true;
	}
	float value = 0.0f;
	if(ReadRealProperty(material, key, semantic, index, value)){
		output = static_cast<std::int32_t>(value);
		return true;
	}
	return false;
}

inline bool ReadStringProperty(
	const aiMaterial& material,
	const char* key,
	unsigned int semantic,
	unsigned int index,
	std::string& output
){
	const aiMaterialProperty* property =
		FindProperty(material, key, semantic, index);
	if(!property || !property->mData ||
		property->mType != aiPTI_String ||
		property->mDataLength < sizeof(unsigned int)){
		return false;
	}

	unsigned int length = 0;
	std::memcpy(&length, property->mData, sizeof(length));
	const std::size_t available = property->mDataLength - sizeof(length);
	const std::size_t safeLength = (std::min)(
		static_cast<std::size_t>(length),
		available
	);
	const char* text = property->mData + sizeof(length);
	output.assign(text, text + safeLength);
	while(!output.empty() && output.back() == '\0'){
		output.pop_back();
	}
	return true;
}

inline bool ReadColorProperty(
	const aiMaterial& material,
	const char* key,
	unsigned int semantic,
	unsigned int index,
	std::array<float, 4>& output
) noexcept {
	const aiMaterialProperty* property =
		FindProperty(material, key, semantic, index);
	if(!property || !property->mData ||
		property->mType != aiPTI_Float ||
		property->mDataLength < sizeof(ai_real) * 3u){
		return false;
	}
	const std::size_t componentCount = (std::min)(
		static_cast<std::size_t>(4),
		static_cast<std::size_t>(
			property->mDataLength / sizeof(ai_real)
		)
	);
	for(std::size_t component = 0;
		component < componentCount;
		++component){
		ai_real value{};
		std::memcpy(
			&value,
			property->mData + component * sizeof(ai_real),
			sizeof(value)
		);
		output[component] = static_cast<float>(value);
	}
	return true;
}

inline bool ReadUVTransformProperty(
	const aiMaterial& material,
	aiTextureType sourceType,
	aiUVTransform& output
) noexcept {
	const aiMaterialProperty* property = FindProperty(
		material,
		_AI_MATKEY_UVTRANSFORM_BASE,
		static_cast<unsigned int>(sourceType),
		0
	);
	if(!property || !property->mData ||
		property->mDataLength < sizeof(aiUVTransform)){
		return false;
	}
	std::memcpy(&output, property->mData, sizeof(output));
	return true;
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
		std::string original;
		if(!ReadStringProperty(
			material,
			_AI_MATKEY_TEXTURE_BASE,
			static_cast<unsigned int>(sourceType),
			0,
			original
		)){
			continue;
		}

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

		std::int32_t uvChannel = 0;
		if(ReadIntegerProperty(
			material,
			_AI_MATKEY_UVWSRC_BASE,
			static_cast<unsigned int>(sourceType),
			0,
			uvChannel
		)){
			binding.uvChannel = static_cast<std::uint8_t>(
				std::clamp(uvChannel, 0, 255)
			);
		}

		float blend = 1.0f;
		if(ReadRealProperty(
			material,
			_AI_MATKEY_TEXBLEND_BASE,
			static_cast<unsigned int>(sourceType),
			0,
			blend
		)){
			binding.strength = blend;
		}

		aiUVTransform transform;
		if(ReadUVTransformProperty(material, sourceType, transform)){
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

		std::int32_t mapping = static_cast<std::int32_t>(aiTextureMapping_UV);
		if(ReadIntegerProperty(
			material,
			_AI_MATKEY_MAPPING_BASE,
			static_cast<unsigned int>(sourceType),
			0,
			mapping
		) && mapping != static_cast<std::int32_t>(aiTextureMapping_UV)){
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

	ReadStringProperty(source, AI_MATKEY_NAME, result.name);
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

	std::array<float, 4> baseColor{1.0f, 1.0f, 1.0f, 1.0f};
	if(!ReadColorProperty(source, AI_MATKEY_BASE_COLOR, baseColor)){
		ReadColorProperty(source, AI_MATKEY_COLOR_DIFFUSE, baseColor);
	}
	result.descriptor.parameters.baseColor = baseColor;

	std::array<float, 4> emissive{0.0f, 0.0f, 0.0f, 1.0f};
	if(ReadColorProperty(source, AI_MATKEY_COLOR_EMISSIVE, emissive)){
		result.descriptor.parameters.emissiveColor = {
			emissive[0],
			emissive[1],
			emissive[2]
		};
	}

	float value = 0.0f;
	if(ReadRealProperty(source, AI_MATKEY_METALLIC_FACTOR, value)){
		result.descriptor.parameters.metallic = std::clamp(value, 0.0f, 1.0f);
	}
	if(ReadRealProperty(source, AI_MATKEY_ROUGHNESS_FACTOR, value)){
		result.descriptor.parameters.roughness = std::clamp(value, 0.0f, 1.0f);
	}
	if(ReadRealProperty(source, AI_MATKEY_OPACITY, value)){
		result.descriptor.parameters.opacity = std::clamp(value, 0.0f, 1.0f);
		result.descriptor.parameters.baseColor[3] *=
			result.descriptor.parameters.opacity;
	}
	if(ReadRealProperty(source, AI_MATKEY_EMISSIVE_INTENSITY, value)){
		result.descriptor.parameters.emissiveIntensity = (std::max)(0.0f, value);
	}else if(emissive[0] != 0.0f || emissive[1] != 0.0f || emissive[2] != 0.0f){
		result.descriptor.parameters.emissiveIntensity = 1.0f;
	}
	if(ReadRealProperty(source, AI_MATKEY_BUMPSCALING, value)){
		result.descriptor.parameters.heightScale = value;
	}

	std::int32_t twoSided = 0;
	if(ReadIntegerProperty(source, AI_MATKEY_TWOSIDED, twoSided) && twoSided != 0){
		result.descriptor.renderState.cullMode = MaterialCullMode::None;
	}

	AppendFirstTexture(source, MaterialTextureSemantic::BaseColor,
		{aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE}, sourceMaterialIndex,
		result.descriptor.textures, diagnostics);
	AppendFirstTexture(source, MaterialTextureSemantic::Normal,
		{aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA}, sourceMaterialIndex,
		result.descriptor.textures, diagnostics);
	AppendFirstTexture(source, MaterialTextureSemantic::Bump,
		{aiTextureType_HEIGHT}, sourceMaterialIndex,
		result.descriptor.textures, diagnostics);
	AppendFirstTexture(source, MaterialTextureSemantic::Height,
		{aiTextureType_DISPLACEMENT}, sourceMaterialIndex,
		result.descriptor.textures, diagnostics);
	AppendFirstTexture(source, MaterialTextureSemantic::Metallic,
		{aiTextureType_METALNESS}, sourceMaterialIndex,
		result.descriptor.textures, diagnostics);
	AppendFirstTexture(source, MaterialTextureSemantic::Roughness,
		{aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_SHININESS}, sourceMaterialIndex,
		result.descriptor.textures, diagnostics);
	AppendFirstTexture(source, MaterialTextureSemantic::AmbientOcclusion,
		{aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP}, sourceMaterialIndex,
		result.descriptor.textures, diagnostics);
	AppendFirstTexture(source, MaterialTextureSemantic::Emissive,
		{aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE}, sourceMaterialIndex,
		result.descriptor.textures, diagnostics);
	const bool hasOpacityTexture = AppendFirstTexture(
		source, MaterialTextureSemantic::Opacity, {aiTextureType_OPACITY},
		sourceMaterialIndex, result.descriptor.textures, diagnostics);

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
		std::string identity;
		ReadStringProperty(*source, AI_MATKEY_NAME, identity);
		const ImportedMaterialID id = materialIDs.Allocate(
			"material", identity, index, &diagnostics);
		materialIDBySourceIndex[index] = id;
		materials.push_back(NormalizeMaterial(
			*source, index, id, diagnostics));
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
			"submesh", identity, index, &diagnostics);
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
