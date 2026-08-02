#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <backends/ImGui/imgui.h>

#include "Backends/ImGuiFunc.h"
#include "Resources/Data/modelData.h"

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

inline void Draw(
	std::vector<ModelSubMeshRenderState>& states,
	const ModelData& model
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
				}
			}

			if(state.material.source ==
				SubMeshMaterialSource::CustomMaterial){
				std::uint32_t customMaterialID =
					state.material.customMaterialID;
				if(ImGui::InputScalar(
					"Custom Material ID",
					ImGuiDataType_U32,
					&customMaterialID
				)){
					state.material.customMaterialID = customMaterialID;
				}
				if(state.material.customMaterialID ==
					InvalidCustomMaterialID){
					ImGui::TextDisabled(
						"ID 0 is invalid; resolver will use Model Default."
					);
				}
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
