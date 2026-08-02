#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

#include "Resources/Data/modelData.h"
#include "Resources/Data/modelMaterialTypes.h"
#include "Shader/Common.hlsl"
#include "Shader/CommonDefine.h"

struct ModelMaterialLegacyTextureLookup {
	const MaterialTextureBinding* binding = nullptr;
	ID3D11ShaderResourceView* texture = nullptr;
	bool runtimeEntryFound = false;

	bool HasReference() const noexcept {
		return binding != nullptr;
	}
};

struct ModelMaterialLegacyD3D11Binding {
	MATERIAL material{};
	std::uint32_t shaderID = 0;
	ModelMaterialLegacyTextureLookup baseColor;
	ModelMaterialLegacyTextureLookup normal;
	ModelMaterialLegacyTextureLookup roughness;
	ModelMaterialLegacyTextureLookup metallic;
	ModelMaterialLegacyTextureLookup ambientOcclusion;
	ModelMaterialLegacyTextureLookup height;
	ModelMaterialLegacyTextureLookup emissive;
};

namespace ModelMaterialLegacyD3D11Bridge {

inline MATERIAL ToLegacyMaterial(
	const MaterialDescriptor& descriptor
) noexcept {
	MATERIAL material{};
	material.BaseColor = float4(
		descriptor.parameters.baseColor[0],
		descriptor.parameters.baseColor[1],
		descriptor.parameters.baseColor[2],
		descriptor.parameters.baseColor[3] *
			descriptor.parameters.opacity
	);
	material.Metallic = descriptor.parameters.metallic;
	material.Roughness = descriptor.parameters.roughness;
	material.AO = descriptor.parameters.ambientOcclusion;
	material.EmissiveColor = float3(
		descriptor.parameters.emissiveColor[0],
		descriptor.parameters.emissiveColor[1],
		descriptor.parameters.emissiveColor[2]
	);
	material.EmissiveIntensity = descriptor.parameters.emissiveIntensity;
	material.MaterialFlags = descriptor.legacyMaterialFlags &
		MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
	return material;
}

inline const MaterialTextureBinding* FindTextureBinding(
	const MaterialDescriptor& descriptor,
	MaterialTextureSemantic semantic
) noexcept {
	for(const MaterialTextureBinding& binding : descriptor.textures){
		if(binding.semantic == semantic){
			return &binding;
		}
	}
	return nullptr;
}

inline std::string_view FileName(std::string_view path) noexcept {
	const std::size_t separator = path.find_last_of("/\\");
	return separator == std::string_view::npos
		? path
		: path.substr(separator + 1);
}

inline bool TryFindTexture(
	const ModelData& model,
	std::string_view path,
	ID3D11ShaderResourceView*& texture
){
	if(path.empty()) return false;
	const auto found = model.m_Texture.find(std::string(path));
	if(found == model.m_Texture.end()) return false;
	texture = found->second;
	return true;
}

inline ModelMaterialLegacyTextureLookup ResolveTexture(
	const ModelData& model,
	const MaterialDescriptor& descriptor,
	MaterialTextureSemantic semantic
){
	ModelMaterialLegacyTextureLookup result;
	result.binding = FindTextureBinding(descriptor, semantic);
	if(!result.binding) return result;

	if(TryFindTexture(model, result.binding->assetPath, result.texture) ||
		TryFindTexture(model, result.binding->sourcePath, result.texture)){
		result.runtimeEntryFound = true;
		return result;
	}

	const std::string_view assetFileName = FileName(result.binding->assetPath);
	const std::string_view sourceFileName = FileName(result.binding->sourcePath);
	if(TryFindTexture(model, assetFileName, result.texture) ||
		TryFindTexture(model, sourceFileName, result.texture)){
		result.runtimeEntryFound = true;
	}
	return result;
}

inline ModelMaterialLegacyD3D11Binding Resolve(
	const ModelData& model,
	const MaterialDescriptor& descriptor
){
	ModelMaterialLegacyD3D11Binding result;
	result.material = ToLegacyMaterial(descriptor);
	result.shaderID = static_cast<std::uint32_t>(
		(std::max)(0, descriptor.shaderID)
	);
	result.baseColor = ResolveTexture(
		model,
		descriptor,
		MaterialTextureSemantic::BaseColor
	);
	result.normal = ResolveTexture(
		model,
		descriptor,
		MaterialTextureSemantic::Normal
	);
	if(!result.normal.HasReference()){
		result.normal = ResolveTexture(
			model,
			descriptor,
			MaterialTextureSemantic::Bump
		);
	}
	result.roughness = ResolveTexture(
		model,
		descriptor,
		MaterialTextureSemantic::Roughness
	);
	result.metallic = ResolveTexture(
		model,
		descriptor,
		MaterialTextureSemantic::Metallic
	);
	result.ambientOcclusion = ResolveTexture(
		model,
		descriptor,
		MaterialTextureSemantic::AmbientOcclusion
	);
	result.height = ResolveTexture(
		model,
		descriptor,
		MaterialTextureSemantic::Height
	);
	result.emissive = ResolveTexture(
		model,
		descriptor,
		MaterialTextureSemantic::Emissive
	);

	if(result.baseColor.texture){
		result.material.MaterialFlags |=
			MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
	}
	if(result.normal.texture){
		result.material.MaterialFlags |=
			MATERIAL_FLAG_USE_NORMAL_TEXTURE;
	}
	if(result.roughness.texture){
		result.material.MaterialFlags |=
			MATERIAL_FLAG_USE_ROUGHNESS_TEXTURE;
	}
	if(result.metallic.texture){
		result.material.MaterialFlags |=
			MATERIAL_FLAG_USE_METALLIC_TEXTURE;
	}
	return result;
}

} // namespace ModelMaterialLegacyD3D11Bridge
