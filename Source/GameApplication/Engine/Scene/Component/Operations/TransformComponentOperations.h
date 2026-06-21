// =======================================================================
//
// TransformComponentOperations.h
//
// TransformComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include <algorithm>
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
	// 保存データ上ではindexを保持し、Sceneロード時に現在のgenerationへ再解決する。
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
		// generationはSceneの参照再マップ処理で補完する。
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

inline void InspectRotation(TransformComponent& transform){
	// Quaternionを毎フレームEulerへ再分解せず、Transformが保持する連続キャッシュを表示する。
	// UIは度、Transform内部はラジアン。
	Vector3 rotationDegrees = RadiansToDegrees(transform.GetRotationEuler());
	const Vector3 beforeFrameRadians = transform.GetRotationEuler();

	const bool changed = ImGui::DragFloat3(
		"Rotation (deg)",
		&rotationDegrees.x,
		0.1f,
		0.0f,
		0.0f,
		"%.3f"
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
							if(target) target->SetRotationEuler(value);
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

inline void Inspect(
	TransformComponent& transform,
	SceneContext* context
){
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

	ImGui::UndoDragVec3("Position", transform.position);
	InspectRotation(transform);

	// Inspector固有状態なのでComponentデータには含めない。
	static bool isUniformScaleLocked = false;
	ImGui::UndoCheckbox("##isUniformLocked", &isUniformScaleLocked);
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip("Lock uniform scale");
	}
	ImGui::SameLine();

	const Vector3 previousScale = transform.scale;
	const bool scaleChanged = ImGui::UndoDragVec3("Scale", transform.scale);

	if(isUniformScaleLocked && scaleChanged){
		if(transform.scale.x != previousScale.x){
			const float ratio = previousScale.x != 0.0f
				? transform.scale.x / previousScale.x
				: 1.0f;
			transform.scale.y = previousScale.y * ratio;
			transform.scale.z = previousScale.z * ratio;
		} else if(transform.scale.y != previousScale.y){
			const float ratio = previousScale.y != 0.0f
				? transform.scale.y / previousScale.y
				: 1.0f;
			transform.scale.x = previousScale.x * ratio;
			transform.scale.z = previousScale.z * ratio;
		} else if(transform.scale.z != previousScale.z){
			const float ratio = previousScale.z != 0.0f
				? transform.scale.z / previousScale.z
				: 1.0f;
			transform.scale.x = previousScale.x * ratio;
			transform.scale.y = previousScale.y * ratio;
		}
	}

	int parentIndex = static_cast<int>(transform.parent.GetIndex());
	if(ImGui::InputInt("Parent Entity", &parentIndex)){
		parentIndex = (std::max)(parentIndex, 0);

		if(parentIndex == 0){
			transform.parent = {};
		} else if(context && context->entity){
			// Entityはindex + generationなので、int*へのreinterpret書き込みは禁止する。
			const Entity resolved = context->entity->Resolve(
				static_cast<uint32_t>(parentIndex)
			);
			if(resolved){
				transform.parent = resolved;
			}
		}
	}

	if(!transform.children.empty()){
		ImGui::Text("Children (%d):", static_cast<int>(transform.children.size()));
		ImGui::Indent();
		for(Entity child : transform.children){
			ImGui::Text(
				"Entity %u (generation %u)",
				child.GetIndex(),
				child.GetGeneration()
			);
		}
		ImGui::Unindent();
	} else {
		ImGui::Text("Children: none");
	}

	ImGui::PopStyleVar();
}

} // namespace TransformComponentOperations

// ComponentRegistryの既存virtual経路を維持する薄い互換ラッパー。
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
