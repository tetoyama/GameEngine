// =======================================================================
//
// MaterialComponentOperations.h
//
// MaterialComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "Backends/ImGuiFunc.h"
#include "Backends/YAMLConverters.h"
#include "Scene/Registry/systemRegistry.h"
#include "Scene/System/Render/RenderSystem/renderSystem.h"
#include "Scene/scene.h"

namespace MaterialComponentOperations {

inline YAML::Node Encode(const MaterialComponent& component){
	YAML::Node node;
	node["ShaderID"] = component.ShaderID;
	node["Material"] = component.Material;
	return node;
}

inline bool Decode(MaterialComponent& component, const YAML::Node& node){
	if(!node.IsMap()) return false;
	if(node["ShaderID"]) component.ShaderID = node["ShaderID"].as<int>();
	if(node["Material"]) component.Material = node["Material"].as<MATERIAL>();
	component.ShaderID = (std::max)(component.ShaderID, 0);
	return true;
}

inline void DrawColorProperty(const char* label, float* channels){
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::SameLine(100.0f);

	const float totalWidth = ImGui::GetContentRegionAvail().x;
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	constexpr float pickerWidth = 24.0f;
	constexpr int sliderCount = 4;
	const float sliderWidth = (std::max)(
		(totalWidth - pickerWidth - spacing * sliderCount) /
		static_cast<float>(sliderCount),
		1.0f
	);

	for(int index = 0; index < sliderCount; ++index){
		ImGui::PushID((std::to_string(index) + label).c_str());
		ImGui::SetNextItemWidth(sliderWidth);
		ImGui::PushStyleColor(
			ImGuiCol_Border,
			ImVec4(
				index == 0 ? 0.7f : 0.0f,
				index == 1 ? 0.7f : 0.0f,
				index == 2 ? 0.7f : 0.0f,
				0.3f
			)
		);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::UndoDragFloat("##Channel", &channels[index], 0.01f, 0.0f, 1.0f);
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		ImGui::PopID();
		ImGui::SameLine();
	}

	const std::string buttonId = std::string("##ColorButton") + label;
	const std::string popupId = std::string("ColorPickerPopup") + label;
	if(ImGui::ColorButton(
		buttonId.c_str(),
		ImVec4(channels[0], channels[1], channels[2], channels[3]),
		ImGuiColorEditFlags_NoTooltip
	)){
		ImGui::OpenPopup(popupId.c_str());
	}
	if(ImGui::BeginPopup(popupId.c_str())){
		ImGui::ColorPicker4(
			(std::string("##ColorPicker") + label).c_str(),
			channels,
			ImGuiColorEditFlags_NoSidePreview |
			ImGuiColorEditFlags_AlphaBar
		);
		ImGui::EndPopup();
	}
}

inline void Inspect(MaterialComponent& component, SceneContext* context){
	ImGui::PushID(&component);

	RenderSystem* renderSystem =
		context && context->system
			? context->system->GetSystem<RenderSystem>()
			: nullptr;

	std::vector<std::string> shaderNames;
	std::vector<const char*> shaderNamePointers;
	if(renderSystem){
		shaderNames.reserve(renderSystem->ShaderMaterials.size());
		for(const ShaderMaterial& material : renderSystem->ShaderMaterials){
			shaderNames.push_back(material.entryPoint);
		}
		shaderNamePointers.reserve(shaderNames.size());
		for(const std::string& name : shaderNames){
			shaderNamePointers.push_back(name.c_str());
		}
	}

	ImGui::TextUnformatted("Shader");
	ImGui::SameLine(100.0f);
	if(!shaderNamePointers.empty()){
		component.ShaderID = (std::clamp)(
			component.ShaderID,
			0,
			static_cast<int>(shaderNamePointers.size()) - 1
		);
		ImGui::Combo(
			"##Shader",
			&component.ShaderID,
			shaderNamePointers.data(),
			static_cast<int>(shaderNamePointers.size())
		);
	} else {
		component.ShaderID = 0;
		ImGui::TextDisabled("No shaders registered");
	}

	DrawColorProperty("BaseColor", &component.Material.BaseColor.x);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Metallic");
	ImGui::SameLine(100.0f);
	ImGui::UndoDragFloat(
		"##Metallic",
		&component.Material.Metallic,
		0.01f,
		0.0f,
		1.0f
	);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Roughness");
	ImGui::SameLine(100.0f);
	ImGui::UndoDragFloat(
		"##Roughness",
		&component.Material.Roughness,
		0.01f,
		0.0f,
		1.0f
	);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("AO");
	ImGui::SameLine(100.0f);
	ImGui::UndoDragFloat("##AO", &component.Material.AO, 0.01f, 0.0f, 1.0f);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Emissive");
	ImGui::SameLine(100.0f);
	ImGui::UndoDragFloat4(
		"##Emissive",
		&component.Material.EmissiveColor.x,
		0.01f,
		0.0f,
		1.0f
	);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Env Map");
	ImGui::SameLine(100.0f);
	bool useEnvironmentMap =
		(component.Material.MaterialFlags & MATERIAL_FLAG_USE_ENVIRONMENT_MAP) != 0;
	if(ImGui::UndoCheckbox("##UseEnvironmentMap", &useEnvironmentMap)){
		if(useEnvironmentMap){
			component.Material.MaterialFlags |= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
		} else {
			component.Material.MaterialFlags &= ~MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
		}
	}

	ImGui::PopID();
}

} // namespace MaterialComponentOperations

inline YAML::Node MaterialComponent::encode(){
	return MaterialComponentOperations::Encode(*this);
}

inline bool MaterialComponent::decode(SceneContext*, const YAML::Node& node){
	return MaterialComponentOperations::Decode(*this, node);
}

inline void MaterialComponent::inspector(SceneContext* context){
	MaterialComponentOperations::Inspect(*this, context);
}
