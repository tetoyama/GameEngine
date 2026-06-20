// =======================================================================
//
// TransformComponentOperations.h
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Backends/ImGuiFunc.h"
#include "Backends/YAMLConverters.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Registry/componentRegistry.h"
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
	// 保存データ上の参照はSceneロード時に現在のgenerationへ再解決する。
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

inline void Inspect(
	TransformComponent& transform,
	SceneContext* context
){
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

	ImGui::UndoDragVec3("Position", transform.position);

	Vector3 rotationEuler = transform.GetRotationEuler();
	if(ImGui::UndoDragVec3("Rotation", rotationEuler)){
		transform.SetRotationEuler(rotationEuler);
	}

	// UI状態でありComponentデータではないため、Operations側に保持する。
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
			// 8byteのEntityへint*を書き込まず、現在のgenerationを安全に解決する。
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

inline void Register(ComponentRegistry& registry){
	registry.SetComponentOperations<TransformComponent>(
		[](const TransformComponent& transform){
			return Encode(transform);
		},
		[](TransformComponent& transform, SceneContext* context, const YAML::Node& node){
			return Decode(transform, context, node);
		},
		[](TransformComponent& transform, SceneContext* context){
			Inspect(transform, context);
		}
	);
}

} // namespace TransformComponentOperations
