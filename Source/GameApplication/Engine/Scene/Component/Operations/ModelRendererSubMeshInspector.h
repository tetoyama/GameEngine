#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <backends/ImGui/imgui.h>

#include "Backends/ImGuiFunc.h"
#include "Resources/Data/modelData.h"
#include "Scene/Component/materialComponent.h"

namespace ModelRendererSubMeshInspector {

inline ModelSubMeshRenderState& EnsureState(
	std::vector<ModelSubMeshRenderState>& states,
	ModelSubMeshID subMeshID
){
	const auto found = std::find_if(
		states.begin(),
		states.end(),
		[subMeshID](const ModelSubMeshRenderState& state){
			return state.subMeshID == subMeshID;
		}
	);
	if(found != states.end()) return *found;
	states.push_back({});
	states.back().subMeshID = subMeshID;
	return states.back();
}

inline const char* ImportedMaterialName(
	const ModelData& model,
	ImportedMaterialID materialID
) noexcept {
	const ImportedMaterialDefinition* material =
		model.FindImportedMaterial(materialID);
	return material && !material->name.empty()
		? material->name.c_str()
		: "Engine Default";
}

inline CustomMaterialID FirstCustomMaterialID(
	const MaterialComponent* materialComponent
) noexcept {
	if(!materialComponent) return InvalidCustomMaterialID;
	for(const CustomMaterialEntry& material : materialComponent->materials){
		if(material.id != InvalidCustomMaterialID){
			return material.id;
		}
	}
	return InvalidCustomMaterialID;
}

inline std::string CustomMaterialDisplayName(
	const CustomMaterialEntry& material
){
	const std::string name = material.name.empty()
		? "Material"
		: material.name;
	return name + " (ID " + std::to_string(material.id) + ")";
}

inline void DrawCustomMaterialAssignment(
	SubMeshMaterialAssignment& assignment,
	const MaterialComponent* materialComponent
){
	const CustomMaterialEntry* selected = materialComponent
		? materialComponent->FindMaterial(assignment.customMaterialID)
		: nullptr;

	std::string preview;
	if(selected){
		preview = CustomMaterialDisplayName(*selected);
	}else if(assignment.customMaterialID == InvalidCustomMaterialID){
		preview = "Unassigned";
	}else{
		preview = "Missing ID " +
			std::to_string(assignment.customMaterialID);
	}

	if(ImGui::BeginCombo("Custom Material", preview.c_str())){
		const bool unassigned =
			assignment.customMaterialID == InvalidCustomMaterialID;
		if(ImGui::Selectable(
			"Unassigned (Model Default fallback)",
			unassigned
		)){
			assignment.customMaterialID = InvalidCustomMaterialID;
		}
		if(unassigned) ImGui::SetItemDefaultFocus();

		if(materialComponent){
			for(const CustomMaterialEntry& material :
				materialComponent->materials){
				if(material.id == InvalidCustomMaterialID) continue;
				const bool isSelected =
					assignment.customMaterialID == material.id;
				const std::string label =
					CustomMaterialDisplayName(material) +
					"##CustomMaterial" + std::to_string(material.id);
				if(ImGui::Selectable(label.c_str(), isSelected)){
					assignment.customMaterialID = material.id;
				}
				if(isSelected) ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if(!materialComponent){
		ImGui::TextDisabled(
			"No MaterialComponent on this Entity; Model Default is used."
		);
	}else if(materialComponent->materials.empty()){
		ImGui::TextDisabled(
			"MaterialComponent has no Custom Material definitions."
		);
	}else if(assignment.customMaterialID != InvalidCustomMaterialID &&
		!selected){
		ImGui::TextDisabled(
			"Selected ID is missing; resolver will use Model Default."
		);
	}
}

inline void Draw(
	std::vector<ModelSubMeshRenderState>& states,
	const ModelData& model,
	const MaterialComponent* materialComponent = nullptr
){
	const std::string label =
		"SubMeshes (" + std::to_string(model.SubMeshes.size()) + ")";
	if(!ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)){
		return;
	}

	if(model.SubMeshes.empty()){
		ImGui::TextDisabled("No normalized submesh definitions.");
		ImGui::TreePop();
		return;
	}

	for(const ModelSubMeshDefinition& definition : model.SubMeshes){
		if(definition.id == InvalidModelSubMeshID) continue;
		ModelSubMeshRenderState& state = EnsureState(states, definition.id);

		ImGui::PushID(static_cast<int>(definition.id));
		const std::string name = definition.name.empty()
			? "SubMesh " + std::to_string(definition.id)
			: definition.name;
		if(ImGui::TreeNode(name.c_str())){
			ImGui::Text("ID: %u", definition.id);
			ImGui::Text("Geometry Index: %u", definition.geometryIndex);
			ImGui::Text(
				"Model Default: %s",
				ImportedMaterialName(model, definition.defaultMaterialID)
			);
			if(!definition.nodePath.empty()){
				ImGui::TextWrapped("Node: %s", definition.nodePath.c_str());
			}

			ImGui::UndoCheckbox("Visible", &state.visible);
			ImGui::UndoCheckbox("Cast Shadow", &state.castShadow);

			static constexpr const char* Sources[] = {
				"Model Default",
				"Custom Material"
			};
			int source = static_cast<int>(state.material.source);
			if(ImGui::Combo("Material Source", &source, Sources, 2)){
				state.material.source =
					static_cast<SubMeshMaterialSource>(source);
				if(state.material.source ==
					SubMeshMaterialSource::ModelDefault){
					state.material.customMaterialID =
						InvalidCustomMaterialID;
				}else if(state.material.customMaterialID ==
					InvalidCustomMaterialID){
					state.material.customMaterialID =
						FirstCustomMaterialID(materialComponent);
				}
			}

			if(state.material.source ==
				SubMeshMaterialSource::CustomMaterial){
				DrawCustomMaterialAssignment(
					state.material,
					materialComponent
				);
			}

			if(ImGui::Button("Reset SubMesh State")){
				state = {};
				state.subMeshID = definition.id;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	ImGui::TreePop();
}

} // namespace ModelRendererSubMeshInspector
