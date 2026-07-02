// =======================================================================
//
// FollowComponentOperations.h
//
// FollowComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include <cstring>
#include <string>
#include <vector>

#include "Backends/ImGuiFunc.h"
#include "Backends/YAMLConverters.h"
#include "Scene/Component/entityNameComponent.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Registry/componentRegistry.h"
#include "Scene/Registry/entityRegistry.h"
#include "Scene/scene.h"

namespace FollowComponentOperations {

inline YAML::Node Encode(const FollowComponent& follow){
	YAML::Node node;
	node["TargetEntity"] = follow.targetEntity.GetIndex();
	node["BoneName"] = follow.boneName;
	node["PositionOffset"] = follow.positionOffset;
	node["FollowPosition"] = follow.followPosition;
	node["FollowRotation"] = follow.followRotation;
	return node;
}

inline bool Decode(
	FollowComponent& follow,
	SceneContext*,
	const YAML::Node& node
){
	if(!node.IsMap()){
		return false;
	}

	if(node["TargetEntity"]){
		// Sceneロード後の参照再マップで現在のgenerationへ解決する。
		follow.targetEntity = Entity(node["TargetEntity"].as<uint32_t>());
	}
	if(node["BoneName"]){
		follow.boneName = node["BoneName"].as<std::string>();
	}
	if(node["PositionOffset"]){
		follow.positionOffset = node["PositionOffset"].as<Vector3>();
	}
	if(node["FollowPosition"]){
		follow.followPosition = node["FollowPosition"].as<bool>();
	}
	if(node["FollowRotation"]){
		follow.followRotation = node["FollowRotation"].as<bool>();
	}
	return true;
}

inline void Inspect(FollowComponent& follow, SceneContext* context){
	ImGui::Text("Target Entity");
	ImGui::SameLine(120.0f);
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

	int entityIndex = static_cast<int>(follow.targetEntity.GetIndex());
	if(ImGui::InputInt("##TargetEntity", &entityIndex)){
		if(entityIndex <= 0){
			follow.targetEntity = {};
		} else if(context && context->entity){
			follow.targetEntity = context->entity->Resolve(
				static_cast<uint32_t>(entityIndex)
			);
		}
	}

	if(follow.targetEntity && context && context->entity){
		if(context->entity->IsAlive(follow.targetEntity)){
			const NameComponent* name =
				context->component->GetComponent<NameComponent>(follow.targetEntity);
			if(name){
				ImGui::SameLine();
				ImGui::TextColored(
					ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
					"[%s]",
					name->name.c_str()
				);
			}
		} else {
			ImGui::SameLine();
			ImGui::TextColored(
				ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
				"[Invalid]"
			);
		}
	}

	bool hasBones = false;
	if(follow.targetEntity &&
		context &&
		context->entity &&
		context->entity->IsAlive(follow.targetEntity)){
		const ModelRendererComponent* modelRenderer =
			context->component->GetComponent<ModelRendererComponent>(
				follow.targetEntity
			);

		if(modelRenderer &&
			modelRenderer->model &&
			!modelRenderer->model->m_BoneIndexMap.empty()){
			hasBones = true;

			ImGui::Text("Bone Name");
			ImGui::SameLine(120.0f);

			std::vector<std::string> boneNames;
			boneNames.reserve(modelRenderer->model->m_BoneIndexMap.size() + 1);
			boneNames.emplace_back();
			for(const auto& [name, index] : modelRenderer->model->m_BoneIndexMap){
				(void)index;
				boneNames.push_back(name);
			}

			int currentIndex = 0;
			for(int i = 0; i < static_cast<int>(boneNames.size()); ++i){
				if(boneNames[i] == follow.boneName){
					currentIndex = i;
					break;
				}
			}

			const char* preview = boneNames[currentIndex].empty()
				? "(None)"
				: boneNames[currentIndex].c_str();

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			if(ImGui::BeginCombo("##BoneName", preview)){
				for(int i = 0; i < static_cast<int>(boneNames.size()); ++i){
					const bool selected = i == currentIndex;
					const char* label = boneNames[i].empty()
						? "(None)"
						: boneNames[i].c_str();
					if(ImGui::Selectable(label, selected)){
						follow.boneName = boneNames[i];
					}
					if(selected){
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}
	}

	if(!hasBones){
		ImGui::Text("Bone Name");
		ImGui::SameLine(120.0f);

		char buffer[256]{};
		std::strncpy(buffer, follow.boneName.c_str(), sizeof(buffer) - 1);
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		if(ImGui::InputText("##BoneName", buffer, sizeof(buffer))){
			follow.boneName = buffer;
		}
	}

	ImGui::UndoDragVec3("Offset", follow.positionOffset);
	ImGui::UndoCheckbox("Follow Position", &follow.followPosition);
	ImGui::SameLine();
	ImGui::UndoCheckbox("Follow Rotation", &follow.followRotation);
}

} // namespace FollowComponentOperations

inline YAML::Node FollowComponent::encode(){
	return FollowComponentOperations::Encode(*this);
}

inline bool FollowComponent::decode(
	SceneContext* context,
	const YAML::Node& node
){
	return FollowComponentOperations::Decode(*this, context, node);
}

inline void FollowComponent::inspector(SceneContext* context){
	FollowComponentOperations::Inspect(*this, context);
}
