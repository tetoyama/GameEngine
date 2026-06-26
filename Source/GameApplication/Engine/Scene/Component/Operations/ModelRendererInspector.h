// =======================================================================
//
// ModelRendererInspector.h
//
// ModelRendererComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "Backends/ImGuiFunc.h"
#include "Scene/scene.h"
#include "Resources/Data/modelData.h"

namespace ModelRendererInspector {

inline bool HasAnimationBinding(
	const ModelRendererComponent& component,
	const std::string& animationName
){
	return std::any_of(
		component.animations.begin(),
		component.animations.end(),
		[&animationName](const auto& animation){
			return animation.first == animationName;
		}
	);
}

inline void BindImportedAnimationsFromPath(
	ModelRendererComponent& component,
	const std::string& path
){
	if(!component.model) return;

	for(const auto& [name, animation] : component.model->m_Animation){
		if(!animation.isImported || animation.FilePath != path ||
			HasAnimationBinding(component, name)){
			continue;
		}
		component.animations.emplace_back(name, path);
	}
}

inline std::vector<std::string> GetAnimationNames(
	const ModelRendererComponent& component
){
	std::vector<std::string> names;
	if(!component.model){
		return names;
	}

	for(const auto& [name, animation] : component.model->m_Animation){
		// Model内蔵Clipは全Entityで利用可能。
		// 外部Import Clipは、このEntityがBindingを所有するものだけを公開する。
		if(!animation.isImported || HasAnimationBinding(component, name)){
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
	if(path == component.modelFilePath){
		return;
	}

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

inline void RemoveAnimation(
	ModelRendererComponent& component,
	const std::string& animationName
){
	// ModelDataはResourceServiceで共有されるため、共有CacheからClipを削除しない。
	// このEntityのBindingとBlend参照だけを除去し、他Entityへ波及させない。
	component.animations.erase(
		std::remove_if(
			component.animations.begin(),
			component.animations.end(),
			[&animationName](const auto& animation){
				return animation.first == animationName;
			}
		),
		component.animations.end()
	);
	component.blendedAnimations.erase(
		std::remove_if(
			component.blendedAnimations.begin(),
			component.blendedAnimations.end(),
			[&animationName](const AnimationBlend& blend){
				return blend.name == animationName;
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
			auto sharedAnimation = component.model->m_Animation.find(name);
			if(sharedAnimation == component.model->m_Animation.end()){
				component.model->LoadAnimation(path.c_str(), name.c_str());
				sharedAnimation = component.model->m_Animation.find(name);
			}

			// 同名Clipは共有Resource内で一意とする。
			// 既存Clipと同じPathの場合のみ、このEntityへBindingを追加できる。
			const bool compatibleSharedClip =
				sharedAnimation != component.model->m_Animation.end() &&
				sharedAnimation->second.isImported &&
				sharedAnimation->second.FilePath == path;
			if(compatibleSharedClip){
				// 1ファイルに複数Clipがある場合も、同じEntityへ全Bindingを保存する。
				BindImportedAnimationsFromPath(component, path);
				animationPath[0] = '\0';
				animationName[0] = '\0';
				ImGui::CloseCurrentPopup();
			}
		}
	}
	ImGui::SameLine();
	if(ImGui::Button("Cancel")){
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

inline void DrawAnimationList(ModelRendererComponent& component){
	if(!component.model){
		return;
	}

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
	std::vector<std::string> animationsToDelete;
	for(const std::string& name : visibleAnimations){
		const auto animationIterator = component.model->m_Animation.find(name);
		if(animationIterator == component.model->m_Animation.end()) continue;
		const AnimationData& animation = animationIterator->second;

		ImGui::PushID(name.c_str());
		const bool canDeleteBinding =
			animation.isImported && HasAnimationBinding(component, name);
		if(!canDeleteBinding){
			ImGui::BeginDisabled();
		}
		if(ImGui::Button("Delete") && canDeleteBinding){
			animationsToDelete.push_back(name);
		}
		if(!canDeleteBinding){
			ImGui::EndDisabled();
		}
		ImGui::SameLine();
		ImGui::TextUnformatted(name.c_str());
		ImGui::PopID();
	}
	for(const std::string& name : animationsToDelete){
		RemoveAnimation(component, name);
	}

	ImGui::TreePop();
}

inline float GetAnimationDuration(
	const ModelRendererComponent& component,
	const std::string& name
){
	if(!component.model){
		return 0.0f;
	}
	const auto iterator = component.model->m_Animation.find(name);
	if(iterator == component.model->m_Animation.end()){
		return 0.0f;
	}
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

inline void DrawMotionBlend(ModelRendererComponent& component){
	const std::string label =
		"Motion Blend(" +
		std::to_string(component.blendedAnimations.size()) +
		")";
	if(!ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)){
		return;
	}

	const std::vector<std::string> animationNames = GetAnimationNames(component);
	if(animationNames.empty()){
		ImGui::TextDisabled("no animations loaded.");
		ImGui::TreePop();
		return;
	}

	static int selectedAnimationIndex = 0;
	selectedAnimationIndex = (std::clamp)(
		selectedAnimationIndex,
		0,
		static_cast<int>(animationNames.size()) - 1
	);

	if(ImGui::Button("Add Animation")){
		const std::string& name = animationNames[selectedAnimationIndex];
		const bool exists = std::any_of(
			component.blendedAnimations.begin(),
			component.blendedAnimations.end(),
			[&name](const AnimationBlend& blend){
				return blend.name == name;
			}
		);
		if(!exists){
			component.blendedAnimations.push_back({name, 0.0f});
		}
	}
	ImGui::SameLine();
	if(ImGui::BeginCombo(
		"##BlendAnimation",
		animationNames[selectedAnimationIndex].c_str()
	)){
		for(int index = 0; index < static_cast<int>(animationNames.size()); ++index){
			const bool selected = index == selectedAnimationIndex;
			if(ImGui::Selectable(animationNames[index].c_str(), selected)){
				selectedAnimationIndex = index;
			}
			if(selected){
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	for(size_t index = 0; index < component.blendedAnimations.size();){
		AnimationBlend& blend = component.blendedAnimations[index];
		ImGui::PushID(static_cast<int>(index));
		if(ImGui::Button("Delete")){
			component.blendedAnimations.erase(
				component.blendedAnimations.begin() + index
			);
			ImGui::PopID();
			continue;
		}
		ImGui::SameLine();
		ImGui::TextUnformatted(blend.name.c_str());

		ImGui::SetNextItemWidth(160.0f);
		ImGui::UndoDragFloat("Weight", &blend.weight, 0.01f, 0.0f, 1.0f);
		const float duration = GetAnimationDuration(component, blend.name);
		ImGui::SetNextItemWidth(160.0f);
		ImGui::UndoDragFloat(
			"StartTime",
			&blend.animationStartTime,
			0.01f,
			0.0f,
			duration
		);
		ImGui::PopID();
		++index;
	}

	ImGui::TreePop();
}

inline void Inspect(
	ModelRendererComponent& component,
	SceneContext* context
){
	ImGui::PushID(&component);
	DrawModelPath(component, context);
	if(component.model){
		DrawAnimationList(component);
		ImGui::Separator();
		DrawMotionBlend(component);
	}
	ImGui::PopID();
}

} // namespace ModelRendererInspector

inline void ModelRendererComponent::inspector(SceneContext* context){
	ModelRendererInspector::Inspect(*this, context);
}
