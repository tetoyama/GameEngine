// =======================================================================
//
// MaterialComponentOperations.h
//
// MaterialComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cfloat>
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
	if(!node.IsMap()){
		return false;
	}
	if(node["ShaderID"]){
		component.ShaderID = node["ShaderID"].as<int>();
	}
	if(node["Material"]){
		component.Material = node["Material"].as<MATERIAL>();
	}
	component.ShaderID = (std::max)(component.ShaderID, 0);
	return true;
}

inline void BeginPropertyRow(const char* label){
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
}

inline void Inspect(MaterialComponent& component, SceneContext* context){
	ImGui::PushID(&component);
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 4.0f));

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

	if(ImGui::BeginTable(
		"MaterialProperties",
		2,
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings |
		ImGuiTableFlags_BordersInnerV
	)){
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 96.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		BeginPropertyRow("Shader");
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

		BeginPropertyRow("Base Color");
		ImGui::UndoColorEdit4(
			"##BaseColor",
			&component.Material.BaseColor.x,
			ImGuiColorEditFlags_Float |
			ImGuiColorEditFlags_AlphaBar |
			ImGuiColorEditFlags_AlphaPreviewHalf
		);

		BeginPropertyRow("Metallic");
		ImGui::UndoDragFloat(
			"##Metallic",
			&component.Material.Metallic,
			0.01f,
			0.0f,
			1.0f
		);

		BeginPropertyRow("Roughness");
		ImGui::UndoDragFloat(
			"##Roughness",
			&component.Material.Roughness,
			0.01f,
			0.0f,
			1.0f
		);

		BeginPropertyRow("Ambient Occlusion");
		ImGui::UndoDragFloat(
			"##AmbientOcclusion",
			&component.Material.AO,
			0.01f,
			0.0f,
			1.0f
		);

		BeginPropertyRow("Emissive");
		ImGui::UndoColorEdit4(
			"##Emissive",
			&component.Material.EmissiveColor.x,
			ImGuiColorEditFlags_Float |
			ImGuiColorEditFlags_HDR |
			ImGuiColorEditFlags_AlphaBar
		);

		BeginPropertyRow("Environment Map");
		bool useEnvironmentMap =
			(component.Material.MaterialFlags & MATERIAL_FLAG_USE_ENVIRONMENT_MAP) != 0;
		if(ImGui::UndoCheckbox("##UseEnvironmentMap", &useEnvironmentMap)){
			if(useEnvironmentMap){
				component.Material.MaterialFlags |= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
			} else {
				component.Material.MaterialFlags &= ~MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
			}
		}

		ImGui::EndTable();
	}

	ImGui::PopStyleVar();
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
