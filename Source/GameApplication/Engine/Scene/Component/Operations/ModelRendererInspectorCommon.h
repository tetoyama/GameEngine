#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "Backends/ImGuiFunc.h"
#include "Scene/scene.h"
#include "Resources/Data/modelData.h"

namespace ModelRendererInspector {

inline bool HasAnimationSourceBinding(
	const ModelRendererComponent& component,
	const std::string& path
){
	return std::any_of(
		component.animations.begin(),
		component.animations.end(),
		[&path](const auto& animation){
			return animation.second == path;
		}
	);
}

inline std::vector<std::string> GetAnimationNames(
	const ModelRendererComponent& component
){
	std::vector<std::string> names;
	if(!component.model) return names;

	for(const auto& [name, animation] : component.model->m_Animation){
		if(!animation.isImported ||
			HasAnimationSourceBinding(component, animation.FilePath)){
			names.push_back(name);
		}
	}
	std::sort(names.begin(), names.end());
	return names;
}

inline void ChangeModelPath(
	ModelRendererComponent& component,
	SceneContext* context,
	const std::string& path
){
	if(path == component.modelFilePath) return;

	component.ReleaseBuffers();
	component.model.reset();
	component.animations.clear();
	component.blendedAnimations.clear();
	component.modelFilePath = path;
	component.animationTime = 0.0f;
	component.CreateModel(context);
}

inline void ReloadCoordinateMode(
	ModelRendererComponent& component,
	SceneContext* context
){
	component.ReleaseBuffers();
	component.model.reset();
	component.CreateModel(context);
}

inline void DrawModelPath(
	ModelRendererComponent& component,
	SceneContext* context
){
	ImGui::TextUnformatted("Model File Path");
	ImGui::SameLine(100.0f);
	if(ImGui::UndoCheckbox("##isBlender", &component.isBlender)){
		ReloadCoordinateMode(component, context);
	}
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip("isBlenderModel?");
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	char pathBuffer[512]{};
	const std::string currentPath = component.model
		? component.model->FilePath
		: component.modelFilePath;
	std::strncpy(pathBuffer, currentPath.c_str(), sizeof(pathBuffer) - 1);

	if(ImGui::InputText("##ModelFilePath", pathBuffer, sizeof(pathBuffer))){
		ChangeModelPath(component, context, pathBuffer);
	}
	if(ImGui::BeginDragDropTarget()){
		if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
			const char* droppedPath = static_cast<const char*>(payload->Data);
			if(droppedPath){
				ChangeModelPath(component, context, droppedPath);
			}
		}
		ImGui::EndDragDropTarget();
	}
}

} // namespace ModelRendererInspector
