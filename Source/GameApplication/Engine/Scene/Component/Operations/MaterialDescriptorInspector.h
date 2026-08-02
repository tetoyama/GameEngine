#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <backends/ImGui/imgui.h>

#include "Backends/ImGuiFunc.h"
#include "CustomMaterialCollection.h"
#include "Scene/System/Render/RenderSystem/ShaderMaterialProvider.h"

namespace MaterialDescriptorInspector {

inline void DrawShaderSelector(
	MaterialDescriptor& descriptor,
	std::span<const ShaderMaterial> shaderMaterials
){
	if(shaderMaterials.empty()){
		descriptor.shaderID = 0;
		ImGui::TextDisabled("No shaders registered");
		return;
	}

	descriptor.shaderID = (std::clamp)(
		descriptor.shaderID,
		0,
		static_cast<int>(shaderMaterials.size()) - 1
	);
	const char* preview =
		shaderMaterials[static_cast<std::size_t>(descriptor.shaderID)]
			.entryPoint.c_str();
	if(ImGui::BeginCombo("Shader", preview)){
		for(std::size_t index = 0; index < shaderMaterials.size(); ++index){
			const bool selected =
				static_cast<std::size_t>(descriptor.shaderID) == index;
			if(ImGui::Selectable(
				shaderMaterials[index].entryPoint.c_str(),
				selected
			)){
				descriptor.shaderID = static_cast<int>(index);
			}
			if(selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

inline void DrawRenderState(MaterialRenderState& state){
	static constexpr const char* AlphaModes[] = {
		"Opaque",
		"Masked",
		"Blend"
	};
	int alphaMode = static_cast<int>(state.alphaMode);
	if(ImGui::Combo("Alpha Mode", &alphaMode, AlphaModes, 3)){
		state.alphaMode = static_cast<MaterialAlphaMode>(alphaMode);
	}
	if(state.alphaMode == MaterialAlphaMode::Masked){
		ImGui::UndoDragFloat(
			"Alpha Cutoff",
			&state.alphaCutoff,
			0.01f,
			0.0f,
			1.0f
		);
	}

	static constexpr const char* CullModes[] = {
		"None",
		"Front",
		"Back"
	};
	int cullMode = static_cast<int>(state.cullMode);
	if(ImGui::Combo("Cull Mode", &cullMode, CullModes, 3)){
		state.cullMode = static_cast<MaterialCullMode>(cullMode);
	}
	ImGui::UndoCheckbox("Depth Write", &state.depthWrite);
	ImGui::UndoCheckbox("Receive Shadow", &state.receiveShadow);
}

inline const char* TextureSemanticName(
	MaterialTextureSemantic semantic
) noexcept {
	switch(semantic){
		case MaterialTextureSemantic::BaseColor: return "Base Color";
		case MaterialTextureSemantic::Normal: return "Normal";
		case MaterialTextureSemantic::Bump: return "Bump";
		case MaterialTextureSemantic::Height: return "Height";
		case MaterialTextureSemantic::Metallic: return "Metallic";
		case MaterialTextureSemantic::Roughness: return "Roughness";
		case MaterialTextureSemantic::AmbientOcclusion: return "Ambient Occlusion";
		case MaterialTextureSemantic::Emissive: return "Emissive";
		case MaterialTextureSemantic::Opacity: return "Opacity";
	}
	return "Unknown";
}

inline void DrawTextureBinding(MaterialTextureBinding& texture){
	static constexpr const char* Semantics[] = {
		"Base Color",
		"Normal",
		"Bump",
		"Height",
		"Metallic",
		"Roughness",
		"Ambient Occlusion",
		"Emissive",
		"Opacity"
	};
	int semantic = static_cast<int>(texture.semantic);
	if(ImGui::Combo("Semantic", &semantic, Semantics, 9)){
		texture.semantic = static_cast<MaterialTextureSemantic>(semantic);
		texture.colorSpace =
			texture.semantic == MaterialTextureSemantic::BaseColor ||
			texture.semantic == MaterialTextureSemantic::Emissive
				? MaterialColorSpace::SRGB
				: MaterialColorSpace::Linear;
	}

	static constexpr const char* ColorSpaces[] = {"Linear", "SRGB"};
	int colorSpace = static_cast<int>(texture.colorSpace);
	if(ImGui::Combo("Color Space", &colorSpace, ColorSpaces, 2)){
		texture.colorSpace = static_cast<MaterialColorSpace>(colorSpace);
	}

	ImGui::UndoInputText("Source Path", &texture.sourcePath, 512);
	ImGui::UndoInputText("Asset Path", &texture.assetPath, 512);

	int uvChannel = static_cast<int>(texture.uvChannel);
	if(ImGui::UndoDragInt("UV Channel", &uvChannel, 1.0f, 0, 255)){
		texture.uvChannel = static_cast<std::uint8_t>(
			(std::clamp)(uvChannel, 0, 255)
		);
	}
	ImGui::UndoDragFloat2(
		"UV Scale",
		texture.uvScale.data(),
		0.01f
	);
	ImGui::UndoDragFloat2(
		"UV Offset",
		texture.uvOffset.data(),
		0.01f
	);
	ImGui::UndoDragFloat(
		"UV Rotation",
		&texture.uvRotation,
		0.01f
	);
	ImGui::UndoDragFloat(
		"Strength",
		&texture.strength,
		0.01f,
		0.0f
	);
	ImGui::UndoCheckbox("Embedded", &texture.embedded);
	if(texture.sourceTextureIndex != InvalidModelSourceIndex){
		ImGui::Text(
			"Source Texture Index: %u",
			texture.sourceTextureIndex
		);
	}
}

inline void DrawTextures(MaterialDescriptor& descriptor){
	const std::string label =
		"Textures (" + std::to_string(descriptor.textures.size()) + ")";
	if(!ImGui::TreeNode(label.c_str())) return;

	if(ImGui::Button("Add Texture")){
		descriptor.textures.emplace_back();
	}

	for(std::size_t index = 0; index < descriptor.textures.size();){
		MaterialTextureBinding& texture = descriptor.textures[index];
		ImGui::PushID(static_cast<int>(index));
		const bool open = ImGui::TreeNode(
			TextureSemanticName(texture.semantic)
		);
		bool remove = false;
		if(open){
			DrawTextureBinding(texture);
			remove = ImGui::Button("Remove Texture");
			ImGui::TreePop();
		}
		ImGui::PopID();
		if(remove){
			descriptor.textures.erase(
				descriptor.textures.begin() +
				static_cast<std::ptrdiff_t>(index)
			);
			continue;
		}
		++index;
	}
	ImGui::TreePop();
}

inline void DrawDescriptor(
	MaterialDescriptor& descriptor,
	std::span<const ShaderMaterial> shaderMaterials
){
	DrawShaderSelector(descriptor, shaderMaterials);
	ImGui::UndoColorEdit4(
		"Base Color",
		descriptor.parameters.baseColor.data(),
		ImGuiColorEditFlags_Float |
		ImGuiColorEditFlags_AlphaBar |
		ImGuiColorEditFlags_AlphaPreviewHalf
	);
	ImGui::UndoDragFloat(
		"Metallic",
		&descriptor.parameters.metallic,
		0.01f,
		0.0f,
		1.0f
	);
	ImGui::UndoDragFloat(
		"Roughness",
		&descriptor.parameters.roughness,
		0.01f,
		0.0f,
		1.0f
	);
	ImGui::UndoDragFloat(
		"Ambient Occlusion",
		&descriptor.parameters.ambientOcclusion,
		0.01f,
		0.0f,
		1.0f
	);
	ImGui::UndoDragFloat3(
		"Emissive Color",
		descriptor.parameters.emissiveColor.data(),
		0.01f,
		0.0f
	);
	ImGui::UndoDragFloat(
		"Emissive Intensity",
		&descriptor.parameters.emissiveIntensity,
		0.01f,
		0.0f
	);
	ImGui::UndoDragFloat(
		"Opacity",
		&descriptor.parameters.opacity,
		0.01f,
		0.0f,
		1.0f
	);
	ImGui::UndoDragFloat(
		"Normal Scale",
		&descriptor.parameters.normalScale,
		0.01f,
		0.0f
	);
	ImGui::UndoDragFloat(
		"Height Scale",
		&descriptor.parameters.heightScale,
		0.01f
	);

	if(ImGui::TreeNode("Render State")){
		DrawRenderState(descriptor.renderState);
		ImGui::TreePop();
	}
	DrawTextures(descriptor);
}

inline void DrawCustomMaterials(
	std::vector<CustomMaterialEntry>& materials,
	std::span<const ShaderMaterial> shaderMaterials
){
	const std::string label =
		"Custom Materials (" + std::to_string(materials.size()) + ")";
	if(!ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)){
		return;
	}

	if(ImGui::Button("Add Custom Material")){
		CustomMaterialCollection::Add(materials);
	}

	for(std::size_t index = 0; index < materials.size();){
		CustomMaterialEntry& material = materials[index];
		ImGui::PushID(static_cast<int>(material.id));
		const std::string materialLabel =
			material.name + "##CustomMaterial";
		const bool open = ImGui::TreeNode(materialLabel.c_str());
		bool remove = false;
		if(open){
			ImGui::Text("ID: %u", material.id);
			ImGui::UndoInputText("Name", &material.name, 256);
			DrawDescriptor(material.inlineMaterial, shaderMaterials);
			remove = ImGui::Button("Remove Custom Material");
			ImGui::TreePop();
		}
		ImGui::PopID();
		if(remove){
			const CustomMaterialID id = material.id;
			CustomMaterialCollection::Remove(materials, id);
			continue;
		}
		++index;
	}
	ImGui::TreePop();
}

} // namespace MaterialDescriptorInspector
