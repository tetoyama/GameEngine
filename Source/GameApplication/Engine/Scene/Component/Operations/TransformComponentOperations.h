// =======================================================================
//
// TransformComponentOperations.h
//
// TransformComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Backends/ImGuiFunc.h"
#include "Backends/YAMLConverters.h"
#include "Editor/Command/CommandManager.h"
#include "Editor/Command/PropertyChangeCommand.h"
#include "Scene/Registry/entityRegistry.h"
#include "Scene/scene.h"

namespace TransformComponentOperations {

inline YAML::Node Encode(const TransformComponent& transform){
	YAML::Node node;
	const DirectX::XMFLOAT4& rotation = transform.GetRotation();

	node["Position"] = transform.position;
	node["Rotation"] = std::vector<float>{
		rotation.x,
		rotation.y,
		rotation.z,
		rotation.w
	};
	node["Scale"] = transform.scale;
	node["Parent"] = transform.parent.GetIndex();
	return node;
}

inline bool Decode(
	TransformComponent& transform,
	SceneContext*,
	const YAML::Node& node
){
	if(!node.IsMap()){
		return false;
	}

	if(node["Position"]){
		transform.position = node["Position"].as<Vector3>();
	}

	if(node["Rotation"] &&
		node["Rotation"].IsSequence() &&
		node["Rotation"].size() == 4){
		const auto values = node["Rotation"].as<std::vector<float>>();
		transform.SetRotation({
			values[0],
			values[1],
			values[2],
			values[3]
		});
	}

	if(node["Scale"]){
		transform.scale = node["Scale"].as<Vector3>();
	}

	if(node["Parent"]){
		transform.parent = Entity(node["Parent"].as<uint32_t>());
	}

	return true;
}

inline Vector3 RadiansToDegrees(const Vector3& radians){
	return Vector3(
		DirectX::XMConvertToDegrees(radians.x),
		DirectX::XMConvertToDegrees(radians.y),
		DirectX::XMConvertToDegrees(radians.z)
	);
}

inline Vector3 DegreesToRadians(const Vector3& degrees){
	return Vector3(
		DirectX::XMConvertToRadians(degrees.x),
		DirectX::XMConvertToRadians(degrees.y),
		DirectX::XMConvertToRadians(degrees.z)
	);
}

inline bool NearlyEqual(
	const Vector3& lhs,
	const Vector3& rhs,
	float epsilon = 1.0e-6f
){
	return std::fabs(lhs.x - rhs.x) <= epsilon &&
		std::fabs(lhs.y - rhs.y) <= epsilon &&
		std::fabs(lhs.z - rhs.z) <= epsilon;
}

inline void BeginPropertyRow(const char* label){
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
}

inline bool InspectVector3(
	const char* id,
	const char* description,
	Vector3& value,
	float speed = 0.01f
){
	const Vector3 beforeFrame = value;
	const bool changed = ImGui::DragFloat3(
		id,
		&value.x,
		speed,
		0.0f,
		0.0f,
		"%.3f"
	);
	const ImGuiID itemID = ImGui::GetItemID();

	struct EditState {
		Vector3* target = nullptr;
		Vector3 before;
	};
	static std::unordered_map<ImGuiID, EditState> editStates;
	EditState& state = editStates[itemID];

	if(ImGui::IsItemActivated()){
		state.target = &value;
		state.before = beforeFrame;
	}

	if(ImGui::IsItemDeactivatedAfterEdit()){
		if(state.target == &value && !NearlyEqual(state.before, value)){
			if(CommandManager* manager = ImGui::GetCommandManager()){
				manager->Push(
					std::make_unique<PropertyChangeCommand<Vector3>>(
						state.target,
						state.before,
						value,
						description
					)
				);
			}
		}
		editStates.erase(itemID);
	}

	return changed;
}

inline void InspectRotation(TransformComponent& transform){
	Vector3 rotationDegrees = RadiansToDegrees(transform.GetRotationEuler());
	const Vector3 beforeFrameRadians = transform.GetRotationEuler();

	const bool changed = ImGui::DragFloat3(
		"##Rotation",
		&rotationDegrees.x,
		0.1f,
		0.0f,
		0.0f,
		"%.3f deg"
	);
	const ImGuiID itemID = ImGui::GetItemID();

	struct RotationEditState {
		TransformComponent* target = nullptr;
		Vector3 beforeRadians;
	};
	static std::unordered_map<ImGuiID, RotationEditState> editStates;
	RotationEditState& state = editStates[itemID];

	if(ImGui::IsItemActivated()){
		state.target = &transform;
		state.beforeRadians = beforeFrameRadians;
	}

	if(changed){
		transform.SetRotationEuler(DegreesToRadians(rotationDegrees));
	}

	if(ImGui::IsItemDeactivatedAfterEdit()){
		const Vector3 afterRadians = transform.GetRotationEuler();
		if(state.target == &transform &&
			!NearlyEqual(state.beforeRadians, afterRadians)){
			if(CommandManager* manager = ImGui::GetCommandManager()){
				TransformComponent* target = state.target;
				manager->Push(
					std::make_unique<PropertyChangeCommandWithSetter<Vector3>>(
						[target](const Vector3& value){
							if(target){
								target->SetRotationEuler(value);
							}
						},
						state.beforeRadians,
						afterRadians,
						"Rotation"
					)
				);
			}
		}
		editStates.erase(itemID);
	}
}

inline bool InspectScale(Vector3& scale, bool uniformLocked){
	const Vector3 beforeFrame = scale;
	bool changed = ImGui::DragFloat3(
		"##Scale",
		&scale.x,
		0.01f,
		0.0f,
		0.0f,
		"%.3f"
	);

	if(changed && uniformLocked){
		if(scale.x != beforeFrame.x){
			const float ratio = beforeFrame.x != 0.0f
				? scale.x / beforeFrame.x
				: 1.0f;
			scale.y = beforeFrame.y * ratio;
			scale.z = beforeFrame.z * ratio;
		} else if(scale.y != beforeFrame.y){
			const float ratio = beforeFrame.y != 0.0f
				? scale.y / beforeFrame.y
				: 1.0f;
			scale.x = beforeFrame.x * ratio;
			scale.z = beforeFrame.z * ratio;
		} else if(scale.z != beforeFrame.z){
			const float ratio = beforeFrame.z != 0.0f
				? scale.z / beforeFrame.z
				: 1.0f;
			scale.x = beforeFrame.x * ratio;
			scale.y = beforeFrame.y * ratio;
		}
	}

	const ImGuiID itemID = ImGui::GetItemID();
	struct ScaleEditState {
		Vector3* target = nullptr;
		Vector3 before;
	};
	static std::unordered_map<ImGuiID, ScaleEditState> editStates;
	ScaleEditState& state = editStates[itemID];

	if(ImGui::IsItemActivated()){
		state.target = &scale;
		state.before = beforeFrame;
	}

	if(ImGui::IsItemDeactivatedAfterEdit()){
		if(state.target == &scale && !NearlyEqual(state.before, scale)){
			if(CommandManager* manager = ImGui::GetCommandManager()){
				manager->Push(
					std::make_unique<PropertyChangeCommand<Vector3>>(
						state.target,
						state.before,
						scale,
						"Scale"
					)
				);
			}
		}
		editStates.erase(itemID);
	}

	return changed;
}

inline void Inspect(
	TransformComponent& transform,
	SceneContext* context
){
	ImGui::PushID(&transform);
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 4.0f));

	if(ImGui::BeginTable(
		"TransformProperties",
		2,
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings |
		ImGuiTableFlags_BordersInnerV
	)){
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 92.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		BeginPropertyRow("Position");
		InspectVector3("##Position", "Position", transform.position);

		BeginPropertyRow("Rotation");
		InspectRotation(transform);

		BeginPropertyRow("Scale");
		ImGuiStorage* storage = ImGui::GetStateStorage();
		const ImGuiID uniformLockID = ImGui::GetID("UniformScaleLock");
		bool uniformLocked = storage->GetBool(uniformLockID, false);
		if(ImGui::Checkbox("##UniformScaleLock", &uniformLocked)){
			storage->SetBool(uniformLockID, uniformLocked);
		}
		if(ImGui::IsItemHovered()){
			ImGui::SetTooltip("Lock uniform scale");
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-FLT_MIN);
		InspectScale(transform.scale, uniformLocked);

		BeginPropertyRow("Parent");
		int parentIndex = static_cast<int>(transform.parent.GetIndex());
		if(ImGui::InputInt("##ParentEntity", &parentIndex)){
			parentIndex = (std::max)(parentIndex, 0);
			if(parentIndex == 0){
				transform.parent = {};
			} else if(context && context->entity){
				const Entity resolved = context->entity->Resolve(
					static_cast<uint32_t>(parentIndex)
				);
				if(resolved){
					transform.parent = resolved;
				}
			}
		}

		ImGui::EndTable();
	}

	if(!transform.children.empty()){
		if(ImGui::TreeNodeEx(
			"Children",
			ImGuiTreeNodeFlags_SpanAvailWidth
		)){
			for(Entity child : transform.children){
				ImGui::BulletText(
					"Entity %u (generation %u)",
					child.GetIndex(),
					child.GetGeneration()
				);
			}
			ImGui::TreePop();
		}
	} else {
		ImGui::TextDisabled("Children: none");
	}

	ImGui::PopStyleVar();
	ImGui::PopID();
}

} // namespace TransformComponentOperations

inline YAML::Node TransformComponent::encode(){
	return TransformComponentOperations::Encode(*this);
}

inline bool TransformComponent::decode(
	SceneContext* context,
	const YAML::Node& node
){
	return TransformComponentOperations::Decode(*this, context, node);
}

inline void TransformComponent::inspector(SceneContext* context){
	TransformComponentOperations::Inspect(*this, context);
}
