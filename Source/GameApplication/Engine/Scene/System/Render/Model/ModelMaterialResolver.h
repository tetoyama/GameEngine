#pragma once

#include <algorithm>
#include <memory>

#include "Resources/Data/modelData.h"
#include "Scene/Component/materialComponent.h"
#include "Scene/Component/modelRendererComponent.h"

struct ModelMaterialResolveResult {
	const MaterialDescriptor* descriptor = nullptr;
	std::shared_ptr<const MaterialDescriptor> ownedDescriptor;
	ModelMaterialResolutionSource source =
		ModelMaterialResolutionSource::EngineDefault;
	ModelMaterialResolutionIssue issue =
		ModelMaterialResolutionIssue::None;
	ModelMaterialResolutionIssue fallbackIssue =
		ModelMaterialResolutionIssue::None;
	ImportedMaterialID importedMaterialID = InvalidImportedMaterialID;
	CustomMaterialID customMaterialID = InvalidCustomMaterialID;
	bool usedFallback = false;

	const MaterialDescriptor* GetDescriptor() const noexcept {
		return ownedDescriptor ? ownedDescriptor.get() : descriptor;
	}

	bool IsResolved() const noexcept {
		return GetDescriptor() != nullptr;
	}
};

namespace ModelMaterialResolver {

inline const MaterialDescriptor& EngineDefaultDescriptor() noexcept {
	static const MaterialDescriptor descriptor{};
	return descriptor;
}

inline const ModelSubMeshRenderState* FindRenderState(
	const ModelRendererComponent* renderer,
	ModelSubMeshID subMeshID
) noexcept {
	return renderer ? renderer->FindSubMeshState(subMeshID) : nullptr;
}

inline bool IsLegacySingleMaterialComponent(
	const MaterialComponent* component
) noexcept {
	return component && component->materials.empty();
}

inline void CanonicalizeDescriptorAlpha(
	MaterialDescriptor& descriptor
) noexcept {
	descriptor.parameters.baseColor[3] *=
		descriptor.parameters.opacity;
	descriptor.parameters.opacity = 1.0f;
}

inline void ApplyLegacySingleMaterialOverride(
	const MaterialComponent& component,
	ModelMaterialResolveResult& result
){
	const MaterialDescriptor* base = result.GetDescriptor();
	MaterialDescriptor descriptor = base
		? *base
		: EngineDefaultDescriptor();

	descriptor.shaderID = (std::max)(0, component.ShaderID);
	descriptor.parameters.baseColor[0] *= component.Material.BaseColor.x;
	descriptor.parameters.baseColor[1] *= component.Material.BaseColor.y;
	descriptor.parameters.baseColor[2] *= component.Material.BaseColor.z;
	descriptor.parameters.baseColor[3] *= component.Material.BaseColor.w;
	descriptor.parameters.metallic = component.Material.Metallic;
	descriptor.parameters.roughness = component.Material.Roughness;
	descriptor.parameters.ambientOcclusion = component.Material.AO;
	descriptor.parameters.emissiveColor = {
		component.Material.EmissiveColor.x,
		component.Material.EmissiveColor.y,
		component.Material.EmissiveColor.z
	};
	descriptor.parameters.emissiveIntensity =
		component.Material.EmissiveIntensity;
	descriptor.legacyMaterialFlags = component.Material.MaterialFlags &
		MATERIAL_FLAG_USE_ENVIRONMENT_MAP;

	result.ownedDescriptor =
		std::make_shared<MaterialDescriptor>(std::move(descriptor));
	result.descriptor = result.ownedDescriptor.get();
	result.source = ModelMaterialResolutionSource::LegacyMaterialOverride;
}

inline bool ResolveImportedDefault(
	const ModelData& model,
	const ModelSubMeshDefinition* subMesh,
	ModelMaterialResolveResult& result,
	bool fallbackFromCustom
) noexcept {
	if(!subMesh){
		result.fallbackIssue =
			ModelMaterialResolutionIssue::MissingSubMeshDefinition;
		return false;
	}
	const ImportedMaterialDefinition* material =
		model.FindImportedMaterial(subMesh->defaultMaterialID);
	if(!material){
		result.fallbackIssue =
			ModelMaterialResolutionIssue::MissingImportedMaterial;
		return false;
	}
	result.descriptor = &material->descriptor;
	result.importedMaterialID = material->id;
	result.source = fallbackFromCustom
		? ModelMaterialResolutionSource::ImportedMaterialFallback
		: ModelMaterialResolutionSource::ImportedMaterial;
	return true;
}

inline ModelMaterialResolveResult Resolve(
	const ModelData& model,
	const ModelSubMeshDefinition* subMesh,
	const ModelSubMeshRenderState* renderState,
	const MaterialComponent* materialComponent
){
	ModelMaterialResolveResult result;
	const SubMeshMaterialAssignment assignment = renderState
		? renderState->material
		: SubMeshMaterialAssignment{};

	if(assignment.source == SubMeshMaterialSource::CustomMaterial){
		result.customMaterialID = assignment.customMaterialID;
		if(!materialComponent){
			result.issue =
				ModelMaterialResolutionIssue::MissingMaterialComponent;
		}else if(const CustomMaterialEntry* custom =
			materialComponent->FindMaterial(assignment.customMaterialID)){
			MaterialDescriptor descriptor = custom->inlineMaterial;
			CanonicalizeDescriptorAlpha(descriptor);
			result.ownedDescriptor =
				std::make_shared<MaterialDescriptor>(std::move(descriptor));
			result.descriptor = result.ownedDescriptor.get();
			result.source = ModelMaterialResolutionSource::CustomMaterial;
			return result;
		}else{
			result.issue =
				ModelMaterialResolutionIssue::MissingCustomMaterial;
		}

		result.usedFallback = true;
		if(ResolveImportedDefault(model, subMesh, result, true)){
			return result;
		}
	}else if(ResolveImportedDefault(model, subMesh, result, false)){
		if(IsLegacySingleMaterialComponent(materialComponent)){
			ApplyLegacySingleMaterialOverride(*materialComponent, result);
		}
		return result;
	}else{
		result.issue = result.fallbackIssue;
	}

	result.descriptor = &EngineDefaultDescriptor();
	result.source = ModelMaterialResolutionSource::EngineDefault;
	result.usedFallback = true;
	if(assignment.source != SubMeshMaterialSource::CustomMaterial &&
		IsLegacySingleMaterialComponent(materialComponent)){
		ApplyLegacySingleMaterialOverride(*materialComponent, result);
	}
	return result;
}

} // namespace ModelMaterialResolver
