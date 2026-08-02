#include <cassert>
#include <cstdint>

#include "Engine/Resources/Data/modelData.h"

int main(){
	static_assert(InvalidModelSubMeshID == 0);
	static_assert(InvalidImportedMaterialID == 0);
	static_assert(InvalidCustomMaterialID == 0);

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

	ModelSubMeshRenderState state;
	state.subMeshID = glass.id;
	state.visible = true;
	state.castShadow = false;
	state.material.source = SubMeshMaterialSource::CustomMaterial;
	state.material.customMaterialID = 30;
	assert(state.subMeshID == 200);
	assert(state.material.customMaterialID == 30);

	CustomMaterialEntry customGlass;
	customGlass.id = 30;
	customGlass.name = "CustomGlass";
	customGlass.inlineMaterial = glassMaterial.descriptor;
	assert(customGlass.inlineMaterial.renderState.alphaMode == MaterialAlphaMode::Blend);

	(void)model;
	return 0;
}
