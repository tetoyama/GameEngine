#pragma once

#include "ModelRendererInspectorCommon.h"

namespace ModelRendererInspector {

inline void RemoveAnimationSource(
	ModelRendererComponent& component,
	const std::string& animationName
){
	if(!component.model) return;

	const auto selected = component.model->m_Animation.find(animationName);
	if(selected == component.model->m_Animation.end() ||
		!selected->second.isImported){
		return;
	}
	const std::string sourcePath = selected->second.FilePath;

	component.animations.erase(
		std::remove_if(
			component.animations.begin(),
			component.animations.end(),
			[&sourcePath](const auto& animation){
				return animation.second == sourcePath;
			}
		),
		component.animations.end()
	);
	component.blendedAnimations.erase(
		std::remove_if(
			component.blendedAnimations.begin(),
			component.blendedAnimations.end(),
			[&component, &sourcePath](const AnimationBlend& blend){
				const auto animation = component.model->m_Animation.find(blend.name);
				return animation != component.model->m_Animation.end() &&
					animation->second.isImported &&
					animation->second.FilePath == sourcePath;
			}
		),
		component.blendedAnimations.end()
	);
}

inline void DrawAddAnimationPopup(ModelRendererComponent& component){
	static char animationPath[512]{};
	static char animationName[128]{};

	if(ImGui::Button("Add Animation")){
		ImGui::OpenPopup("AddAnimationPopup");
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	ImGui::InputText("##AnimationPath", animationPath, sizeof(animationPath));
	if(ImGui::BeginDragDropTarget()){
		if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
			const char* droppedPath = static_cast<const char*>(payload->Data);
			if(droppedPath){
				std::strncpy(animationPath, droppedPath, sizeof(animationPath) - 1);
				ImGui::OpenPopup("AddAnimationPopup");
			}
		}
		ImGui::EndDragDropTarget();
	}

	if(!ImGui::BeginPopupModal(
		"AddAnimationPopup",
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize
	)){
		return;
	}

	ImGui::InputText("File Path", animationPath, sizeof(animationPath));
	if(ImGui::BeginDragDropTarget()){
		if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
			const char* droppedPath = static_cast<const char*>(payload->Data);
			if(droppedPath){
				std::strncpy(animationPath, droppedPath, sizeof(animationPath) - 1);
			}
		}
		ImGui::EndDragDropTarget();
	}
	ImGui::InputText("Name", animationName, sizeof(animationName));

	if(ImGui::Button("Add")){
		const std::string path(animationPath);
		const std::string name(animationName);
		if(component.model && !path.empty() && !name.empty()){
			// 共有ModelDataはInspectorから変更しない。
			// 次のMain Thread Animation Upload TaskがBindingをImportする。
			if(!HasAnimationSourceBinding(component, path)){
				component.animations.emplace_back(name, path);
			}
			animationPath[0] = '\0';
			animationName[0] = '\0';
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::SameLine();
	if(ImGui::Button("Cancel")){
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

inline void DrawAnimationList(ModelRendererComponent& component){
	if(!component.model) return;

	ImGui::Separator();
	ImGui::UndoDragFloat("Animation Time", &component.animationTime, 1.0f, 0.0f);

	const std::vector<std::string> visibleAnimations = GetAnimationNames(component);
	const std::string label =
		"LoadedAnimation(" +
		std::to_string(visibleAnimations.size()) +
		")";
	if(!ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)){
		return;
	}

	DrawAddAnimationPopup(component);
	std::vector<std::string> sourcesToDelete;
	for(const std::string& name : visibleAnimations){
		const auto animationIterator = component.model->m_Animation.find(name);
		if(animationIterator == component.model->m_Animation.end()) continue;
		const AnimationData& animation = animationIterator->second;

		ImGui::PushID(name.c_str());
		const bool canDeleteSource =
			animation.isImported &&
			HasAnimationSourceBinding(component, animation.FilePath);
		if(!canDeleteSource) ImGui::BeginDisabled();
		if(ImGui::Button("Delete") && canDeleteSource){
			sourcesToDelete.push_back(name);
		}
		if(!canDeleteSource) ImGui::EndDisabled();
		if(canDeleteSource && ImGui::IsItemHovered()){
			ImGui::SetTooltip("Removes this animation source from the entity.");
		}
		ImGui::SameLine();
		ImGui::TextUnformatted(name.c_str());
		ImGui::PopID();
	}
	for(const std::string& name : sourcesToDelete){
		RemoveAnimationSource(component, name);
	}
	ImGui::TreePop();
}

inline float GetAnimationDuration(
	const ModelRendererComponent& component,
	const std::string& name
){
	if(!component.model) return 0.0f;
	const auto iterator = component.model->m_Animation.find(name);
	if(iterator == component.model->m_Animation.end()) return 0.0f;

	const AnimationData& animation = iterator->second;
	if(animation.Animation){
		return static_cast<float>(animation.Animation->mDuration);
	}
	if(animation.Scene && animation.Scene->mNumAnimations > 0 &&
		animation.Scene->mAnimations[0]){
		return static_cast<float>(animation.Scene->mAnimations[0]->mDuration);
	}
	return 0.0f;
}

} // namespace ModelRendererInspector
