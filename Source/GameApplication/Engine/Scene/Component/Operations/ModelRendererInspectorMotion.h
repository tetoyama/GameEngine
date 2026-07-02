#pragma once

#include "ModelRendererInspectorAnimation.h"

namespace ModelRendererInspector {

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
			if(selected) ImGui::SetItemDefaultFocus();
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
