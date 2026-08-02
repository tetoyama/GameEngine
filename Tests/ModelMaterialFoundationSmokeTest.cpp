#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "Engine/Resources/Data/modelData.h"
#include "Engine/Resources/Data/modelAssimpMaterialPropertyNormalization.h"
#include "Engine/Scene/Component/materialComponent.h"
#include "Engine/Scene/System/Render/Model/ModelMaterialLegacyD3D11Bridge.h"
#include "Engine/Scene/System/Render/Model/ModelMaterialResolver.h"

int main(){
	static_assert(InvalidModelSubMeshID == 0);
	static_assert(InvalidImportedMaterialID == 0);
	static_assert(InvalidCustomMaterialID == 0);

	using namespace ModelAssimpMaterialNormalization;
	assert(NormalizeTexturePath("./Textures\\Body//BaseColor.png") ==
		"Textures/Body/BaseColor.png");
	assert(ParseEmbeddedTextureIndex("*17") == 17);
	assert(ParseEmbeddedTextureIndex("Texture.png") == InvalidModelSourceIndex);
	assert(ColorSpaceFor(MaterialTextureSemantic::BaseColor) ==
		MaterialColorSpace::SRGB);
	assert(ColorSpaceFor(MaterialTextureSemantic::Normal) ==
		MaterialColorSpace::Linear);

	std::vector<ModelMaterialImportDiagnostic> idDiagnostics;
	StableLocalIDAllocator ids;
	const std::uint32_t bodyID = ids.Allocate(
		"material",
		"Body",
		0,
		&idDiagnostics
	);
	const std::uint32_t bodyIDCollision = ids.Allocate(
		"material",
		"Body",
		0,
		&idDiagnostics
	);
	assert(bodyID != 0);
	assert(bodyIDCollision != 0);
	assert(bodyID != bodyIDCollision);
	assert(idDiagnostics.size() == 1);
	assert(idDiagnostics[0].code ==
		ModelMaterialImportDiagnosticCode::StableLocalIDCollision);

	MaterialDescriptor descriptor;
	descriptor.shaderID = 4;
	descriptor.parameters.baseColor = {0.25f, 0.5f, 0.75f, 1.0f};
	descriptor.parameters.metallic = 0.8f;
	descriptor.parameters.roughness = 0.3f;
	descriptor.renderState.alphaMode = MaterialAlphaMode::Masked;
	descriptor.renderState.alphaCutoff = 0.4f;

	MaterialTextureBinding baseColor;
	baseColor.semantic = MaterialTextureSemantic::BaseColor;
	baseColor.colorSpace = MaterialColorSpace::SRGB;
	baseColor.sourcePath = ".\\Textures\\Body_BaseColor.png";
	baseColor.assetPath = "Textures/Body_BaseColor.png";

	MaterialTextureBinding normal;
	normal.semantic = MaterialTextureSemantic::Normal;
	normal.colorSpace = MaterialColorSpace::Linear;
	normal.assetPath = "Textures/Body_Normal.png";
	normal.uvChannel = 1;
	descriptor.textures.push_back(baseColor);
	descriptor.textures.push_back(normal);

	assert(descriptor.textures.size() == 2);
	assert(descriptor.textures[0].colorSpace == MaterialColorSpace::SRGB);
	assert(descriptor.textures[1].colorSpace == MaterialColorSpace::Linear);

	// ModelDataのDestructorはmodelData.cppに実装されているため、この単一TUの
	// Header contract testではProcess終了まで保持する。
	ModelData* model = new ModelData();
	model->MeshGeometry.resize(3);
	assert(model->ResolvedSubMeshCount() == 3);

	ImportedMaterialDefinition bodyMaterial;
	bodyMaterial.id = 10;
	bodyMaterial.name = "Body";
	bodyMaterial.sourceMaterialIndex = 0;
	bodyMaterial.descriptor = descriptor;
	model->ImportedMaterials.push_back(bodyMaterial);

	ImportedMaterialDefinition glassMaterial;
	glassMaterial.id = 20;
	glassMaterial.name = "Glass";
	glassMaterial.sourceMaterialIndex = 1;
	glassMaterial.descriptor.shaderID = 7;
	glassMaterial.descriptor.renderState.alphaMode = MaterialAlphaMode::Blend;
	model->ImportedMaterials.push_back(glassMaterial);

	ModelSubMeshDefinition body;
	body.id = 100;
	body.name = "Body";
	body.nodePath = "Root/Body";
	body.geometryIndex = 2;
	body.defaultMaterialID = bodyMaterial.id;
	model->SubMeshes.push_back(body);

	ModelSubMeshDefinition glass;
	glass.id = 200;
	glass.name = "Glass";
	glass.nodePath = "Root/Glass";
	glass.geometryIndex = 0;
	glass.defaultMaterialID = glassMaterial.id;
	model->SubMeshes.push_back(glass);

	// 正規化済みSubMesh定義が存在する場合、Legacy Geometry配列の要素数ではなく
	// SubMesh定義を描画Section数として使用する。
	assert(model->ResolvedSubMeshCount() == 2);
	assert(model->FindSubMesh(100) == &model->SubMeshes[0]);
	assert(model->FindSubMesh(200) == &model->SubMeshes[1]);
	assert(model->FindSubMesh(999) == nullptr);
	assert(model->FindImportedMaterial(10) == &model->ImportedMaterials[0]);
	assert(model->FindImportedMaterial(20) == &model->ImportedMaterials[1]);
	assert(model->FindImportedMaterial(999) == nullptr);

	// 旧単一MaterialComponentは新Custom Materialと混同せず、Imported Defaultへ
	// 明示的に重ねる移行Overrideとして保持する。
	MaterialComponent legacyComponent;
	legacyComponent.ShaderID = 11;
	legacyComponent.Material.BaseColor = float4(0.5f, 0.25f, 1.0f, 0.8f);
	legacyComponent.Material.Metallic = 0.4f;
	legacyComponent.Material.Roughness = 0.6f;
	legacyComponent.Material.AO = 0.7f;
	legacyComponent.Material.EmissiveColor = float3(0.1f, 0.2f, 0.3f);
	legacyComponent.Material.EmissiveIntensity = 2.0f;
	legacyComponent.Material.MaterialFlags = MATERIAL_FLAG_USE_ENVIRONMENT_MAP;

	const ModelMaterialResolveResult legacyResolved =
		ModelMaterialResolver::Resolve(
			*model,
			model->FindSubMesh(body.id),
			nullptr,
			&legacyComponent
		);
	assert(legacyResolved.IsResolved());
	assert(legacyResolved.source ==
		ModelMaterialResolutionSource::LegacyMaterialOverride);
	assert(legacyResolved.GetDescriptor()->shaderID == 11);
	assert(legacyResolved.GetDescriptor()->parameters.baseColor[0] == 0.125f);
	assert(legacyResolved.GetDescriptor()->parameters.baseColor[1] == 0.125f);
	assert(legacyResolved.GetDescriptor()->parameters.baseColor[2] == 0.75f);
	assert(legacyResolved.GetDescriptor()->parameters.metallic == 0.4f);
	assert(legacyResolved.GetDescriptor()->parameters.roughness == 0.6f);
	assert(legacyResolved.GetDescriptor()->legacyMaterialFlags ==
		MATERIAL_FLAG_USE_ENVIRONMENT_MAP);

	ID3D11ShaderResourceView* fakeDiffuse =
		reinterpret_cast<ID3D11ShaderResourceView*>(static_cast<std::uintptr_t>(1));
	model->m_Texture["Textures/Body_BaseColor.png"] = fakeDiffuse;
	const ModelMaterialLegacyD3D11Binding legacyBinding =
		ModelMaterialLegacyD3D11Bridge::Resolve(
			*model,
			*legacyResolved.GetDescriptor()
		);
	assert(legacyBinding.shaderID == 11);
	assert(legacyBinding.baseColor.runtimeEntryFound);
	assert(legacyBinding.baseColor.texture == fakeDiffuse);
	assert((legacyBinding.material.MaterialFlags &
		MATERIAL_FLAG_USE_DIFFUSE_TEXTURE) != 0);
	assert((legacyBinding.material.MaterialFlags &
		MATERIAL_FLAG_USE_ENVIRONMENT_MAP) != 0);
	assert(legacyBinding.material.Metallic == 0.4f);
	assert(legacyBinding.material.Roughness == 0.6f);

	ModelSubMeshRenderState state;
	state.subMeshID = glass.id;
	state.visible = true;
	state.castShadow = false;
	state.material.source = SubMeshMaterialSource::CustomMaterial;
	state.material.customMaterialID = 30;

	MaterialComponent materialComponent;
	CustomMaterialEntry customGlass;
	customGlass.id = 30;
	customGlass.name = "CustomGlass";
	customGlass.inlineMaterial = glassMaterial.descriptor;
	customGlass.inlineMaterial.shaderID = 9;
	materialComponent.materials.push_back(customGlass);

	const ModelMaterialResolveResult customResolved =
		ModelMaterialResolver::Resolve(
			*model,
			model->FindSubMesh(glass.id),
			&state,
			&materialComponent
		);
	assert(customResolved.IsResolved());
	assert(customResolved.source == ModelMaterialResolutionSource::CustomMaterial);
	assert(customResolved.issue == ModelMaterialResolutionIssue::None);
	assert(customResolved.customMaterialID == 30);
	assert(customResolved.GetDescriptor()->shaderID == 9);

	state.material.customMaterialID = 999;
	const ModelMaterialResolveResult brokenCustom =
		ModelMaterialResolver::Resolve(
			*model,
			model->FindSubMesh(glass.id),
			&state,
			&materialComponent
		);
	assert(brokenCustom.IsResolved());
	assert(brokenCustom.usedFallback);
	assert(brokenCustom.issue ==
		ModelMaterialResolutionIssue::MissingCustomMaterial);
	assert(brokenCustom.source ==
		ModelMaterialResolutionSource::ImportedMaterialFallback);
	assert(brokenCustom.importedMaterialID == glassMaterial.id);
	assert(brokenCustom.GetDescriptor()->shaderID == 7);

	const ModelMaterialResolveResult missingComponent =
		ModelMaterialResolver::Resolve(
			*model,
			model->FindSubMesh(glass.id),
			&state,
			nullptr
		);
	assert(missingComponent.usedFallback);
	assert(missingComponent.issue ==
		ModelMaterialResolutionIssue::MissingMaterialComponent);
	assert(missingComponent.source ==
		ModelMaterialResolutionSource::ImportedMaterialFallback);

	ModelSubMeshDefinition brokenSubMesh = glass;
	brokenSubMesh.defaultMaterialID = 12345;
	const ModelMaterialResolveResult engineFallback =
		ModelMaterialResolver::Resolve(
			*model,
			&brokenSubMesh,
			nullptr,
			nullptr
		);
	assert(engineFallback.IsResolved());
	assert(engineFallback.usedFallback);
	assert(engineFallback.source ==
		ModelMaterialResolutionSource::EngineDefault);
	assert(engineFallback.issue ==
		ModelMaterialResolutionIssue::MissingImportedMaterial);

	(void)model;
	return 0;
}