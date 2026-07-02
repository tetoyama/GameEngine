// =======================================================================
//
// ColliderComponentOperations.h
//
// ColliderComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "Backends/ImGuiFunc.h"
#include "Backends/YAMLConverters.h"
#include "Scene/scene.h"
#include "Scene/Registry/componentRegistry.h"
#include "Scene/Registry/systemRegistry.h"
#include "Scene/System/Physic/physicSystem.h"
#include "Scene/Component/modelRendererComponent.h"

namespace ColliderComponentOperations {

inline YAML::Node EncodeVector3(const Vector3& value){
	YAML::Node node;
	node.push_back(value.x);
	node.push_back(value.y);
	node.push_back(value.z);
	return node;
}

inline bool DecodeVector3(const YAML::Node& node, Vector3& value){
	if(!node || !node.IsSequence() || node.size() < 3){
		return false;
	}
	value = Vector3(
		node[0].as<float>(),
		node[1].as<float>(),
		node[2].as<float>()
	);
	return true;
}

inline YAML::Node Encode(const ColliderComponent& component){
	YAML::Node node;
	node["isDynamic"] = component.isDynamic;
	node["autoMass"] = component.autoMass;
	node["Mass"] = component.Mass;

	for(const ColliderShape& collider : component.colliders){
		YAML::Node colliderNode;
		colliderNode["type"] = static_cast<int>(collider.type);
		colliderNode["size"] = EncodeVector3(collider.size);
		colliderNode["offset"] = EncodeVector3(collider.offset);
		colliderNode["rotationOffset"] = EncodeVector3(collider.rotationOffset);
		colliderNode["radius"] = collider.radius;
		colliderNode["height"] = collider.height;
		colliderNode["staticFriction"] = collider.staticFriction;
		colliderNode["dynamicFriction"] = collider.dynamicFriction;
		colliderNode["restitution"] = collider.restitution;
		colliderNode["collisionLayer"] = collider.collisionLayer;
		colliderNode["lockRotX"] = collider.lockRotX;
		colliderNode["lockRotY"] = collider.lockRotY;
		colliderNode["lockRotZ"] = collider.lockRotZ;
		colliderNode["isTrigger"] = collider.isTrigger;
		colliderNode["boneName"] = collider.boneName;
		node["colliders"].push_back(colliderNode);
	}

	return node;
}

inline bool Decode(ColliderComponent& component, const YAML::Node& node){
	if(!node.IsMap()){
		return false;
	}

	if(node["isDynamic"]){
		component.isDynamic = node["isDynamic"].as<bool>();
	}
	if(node["autoMass"]){
		component.autoMass = node["autoMass"].as<bool>();
	}
	if(node["Mass"]){
		component.Mass = node["Mass"].as<float>();
	}

	component.colliders.clear();
	if(node["colliders"] && node["colliders"].IsSequence()){
		for(const YAML::Node& colliderNode : node["colliders"]){
			ColliderShape collider;
			if(colliderNode["type"]){
				collider.type = static_cast<ColliderType>(
					colliderNode["type"].as<int>()
				);
			}
			DecodeVector3(colliderNode["size"], collider.size);
			DecodeVector3(colliderNode["offset"], collider.offset);
			DecodeVector3(colliderNode["rotationOffset"], collider.rotationOffset);
			if(colliderNode["radius"]){
				collider.radius = colliderNode["radius"].as<float>();
			}
			if(colliderNode["height"]){
				collider.height = colliderNode["height"].as<float>();
			}
			if(colliderNode["staticFriction"]){
				collider.staticFriction = colliderNode["staticFriction"].as<float>();
			}
			if(colliderNode["dynamicFriction"]){
				collider.dynamicFriction = colliderNode["dynamicFriction"].as<float>();
			}
			if(colliderNode["restitution"]){
				collider.restitution = colliderNode["restitution"].as<float>();
			}
			if(colliderNode["collisionLayer"]){
				collider.collisionLayer = colliderNode["collisionLayer"].as<uint32_t>();
			}
			if(colliderNode["lockRotX"]){
				collider.lockRotX = colliderNode["lockRotX"].as<bool>();
			}
			if(colliderNode["lockRotY"]){
				collider.lockRotY = colliderNode["lockRotY"].as<bool>();
			}
			if(colliderNode["lockRotZ"]){
				collider.lockRotZ = colliderNode["lockRotZ"].as<bool>();
			}
			if(colliderNode["isTrigger"]){
				collider.isTrigger = colliderNode["isTrigger"].as<bool>();
			}
			if(colliderNode["boneName"]){
				collider.boneName = colliderNode["boneName"].as<std::string>();
			}

			collider.size.x = (std::max)(collider.size.x, 0.001f);
			collider.size.y = (std::max)(collider.size.y, 0.001f);
			collider.size.z = (std::max)(collider.size.z, 0.001f);
			collider.radius = (std::max)(collider.radius, 0.001f);
			collider.height = (std::max)(collider.height, 0.001f);
			collider.staticFriction = (std::clamp)(collider.staticFriction, 0.0f, 1.0f);
			collider.dynamicFriction = (std::clamp)(collider.dynamicFriction, 0.0f, 1.0f);
			collider.restitution = (std::clamp)(collider.restitution, 0.0f, 1.0f);
			component.colliders.push_back(std::move(collider));
		}
	}

	component.Mass = (std::max)(component.Mass, 0.001f);
	component.needsUpdate = true;
	return true;
}

inline ModelRendererComponent* FindModelRenderer(
	ColliderComponent& component,
	SceneContext* context
){
	if(!context || !context->component){
		return nullptr;
	}

	const auto entities =
		context->component->FindEntitiesWithComponent<ColliderComponent>();
	for(Entity entity : entities){
		if(context->component->GetComponent<ColliderComponent>(entity) == &component){
			return context->component->GetComponent<ModelRendererComponent>(entity);
		}
	}
	return nullptr;
}

inline void BeginPropertyRow(const char* label){
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
}

inline bool BeginPropertyTable(const char* id){
	if(!ImGui::BeginTable(
		id,
		2,
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings |
		ImGuiTableFlags_BordersInnerV
	)){
		return false;
	}

	ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 112.0f);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
	return true;
}

inline void DrawGeneralSettings(ColliderComponent& component){
	bool changed = false;

	if(BeginPropertyTable("ColliderGeneralProperties")){
		BeginPropertyRow("Dynamic");
		changed |= ImGui::UndoCheckbox("##Dynamic", &component.isDynamic);

		BeginPropertyRow("Automatic Mass");
		if(!component.isDynamic){
			ImGui::BeginDisabled();
		}
		changed |= ImGui::UndoCheckbox("##AutomaticMass", &component.autoMass);
		if(!component.isDynamic){
			ImGui::EndDisabled();
		}

		BeginPropertyRow("Mass");
		if(!component.isDynamic || component.autoMass){
			ImGui::BeginDisabled();
		}
		changed |= ImGui::UndoDragFloat(
			"##Mass",
			&component.Mass,
			0.1f,
			0.001f,
			1000000.0f
		);
		if(!component.isDynamic || component.autoMass){
			ImGui::EndDisabled();
		}

		ImGui::EndTable();
	}

	component.Mass = (std::max)(component.Mass, 0.001f);
	component.needsUpdate |= changed;
}

inline bool DrawCollisionLayer(ColliderShape& collider, SceneContext* context){
	PhysicSystem* physics = nullptr;
	if(context && context->system){
		physics = context->system->GetSystem<PhysicSystem>();
	}

	if(!physics || physics->GetLayers().empty()){
		ImGui::TextDisabled("No collision layers");
		return false;
	}

	const auto& layers = physics->GetLayers();
	int currentIndex = -1;
	for(int index = 0; index < static_cast<int>(layers.size()); ++index){
		if(collider.collisionLayer == layers[index].bit){
			currentIndex = index;
			break;
		}
	}

	const char* preview = currentIndex >= 0
		? layers[currentIndex].name.c_str()
		: "Unassigned";
	bool changed = false;
	if(ImGui::BeginCombo("##CollisionLayer", preview)){
		for(int index = 0; index < static_cast<int>(layers.size()); ++index){
			const bool selected = index == currentIndex;
			if(ImGui::Selectable(layers[index].name.c_str(), selected)){
				collider.collisionLayer = layers[index].bit;
				changed = true;
			}
			if(selected){
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}

inline bool DrawBoneName(
	ColliderComponent& component,
	ColliderShape& collider,
	SceneContext* context
){
	ModelRendererComponent* modelRenderer = FindModelRenderer(component, context);
	const char* preview = collider.boneName.empty()
		? "None"
		: collider.boneName.c_str();
	bool changed = false;

	if(modelRenderer && modelRenderer->model &&
		!modelRenderer->model->m_BoneIndexMap.empty()){
		if(ImGui::BeginCombo("##BoneName", preview)){
			if(ImGui::Selectable("None", collider.boneName.empty())){
				collider.boneName.clear();
				changed = true;
			}

			std::vector<std::string> boneNames;
			boneNames.reserve(modelRenderer->model->m_BoneIndexMap.size());
			for(const auto& [name, boneIndex] : modelRenderer->model->m_BoneIndexMap){
				(void)boneIndex;
				boneNames.push_back(name);
			}
			std::sort(boneNames.begin(), boneNames.end());

			for(const std::string& boneName : boneNames){
				const bool selected = collider.boneName == boneName;
				if(ImGui::Selectable(boneName.c_str(), selected)){
					collider.boneName = boneName;
					changed = true;
				}
				if(selected){
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	char buffer[128]{};
	std::strncpy(buffer, collider.boneName.c_str(), sizeof(buffer) - 1);
	if(ImGui::InputText("##BoneName", buffer, sizeof(buffer))){
		collider.boneName = buffer;
		changed = true;
	}
	return changed;
}

inline void DrawShape(
	ColliderComponent& component,
	ColliderShape& collider,
	SceneContext* context
){
	bool changed = false;

	if(ImGui::TreeNodeEx(
		"Shape",
		ImGuiTreeNodeFlags_DefaultOpen |
		ImGuiTreeNodeFlags_SpanAvailWidth
	)){
		if(BeginPropertyTable("ColliderShapeProperties")){
			BeginPropertyRow("Type");
			int type = static_cast<int>(collider.type);
			if(ImGui::Combo(
				"##Type",
				&type,
				"Box\0Sphere\0Capsule\0Mesh\0Height Map\0"
			)){
				collider.type = static_cast<ColliderType>(type);
				changed = true;
			}

			BeginPropertyRow("Trigger");
			changed |= ImGui::UndoCheckbox("##Trigger", &collider.isTrigger);

			BeginPropertyRow("Collision Layer");
			changed |= DrawCollisionLayer(collider, context);

			if(collider.type == ColliderType::Box){
				BeginPropertyRow("Size");
				changed |= ImGui::UndoDragFloat3(
					"##Size",
					&collider.size.x,
					0.01f,
					0.001f,
					1000000.0f
				);
			} else if(collider.type == ColliderType::Sphere){
				BeginPropertyRow("Radius");
				changed |= ImGui::UndoDragFloat(
					"##Radius",
					&collider.radius,
					0.01f,
					0.001f,
					1000000.0f
				);
			} else if(collider.type == ColliderType::Capsule){
				BeginPropertyRow("Radius");
				changed |= ImGui::UndoDragFloat(
					"##Radius",
					&collider.radius,
					0.01f,
					0.001f,
					1000000.0f
				);

				BeginPropertyRow("Height");
				changed |= ImGui::UndoDragFloat(
					"##Height",
					&collider.height,
					0.01f,
					0.001f,
					1000000.0f
				);
			}

			ImGui::EndTable();
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNodeEx(
		"Local Transform",
		ImGuiTreeNodeFlags_DefaultOpen |
		ImGuiTreeNodeFlags_SpanAvailWidth
	)){
		if(BeginPropertyTable("ColliderTransformProperties")){
			BeginPropertyRow("Bone");
			changed |= DrawBoneName(component, collider, context);

			BeginPropertyRow("Position");
			changed |= ImGui::UndoDragFloat3(
				"##Position",
				&collider.offset.x,
				0.01f
			);

			BeginPropertyRow("Rotation");
			changed |= ImGui::UndoDragFloat3(
				"##RotationDegrees",
				&collider.rotationOffset.x,
				0.1f,
				0.0f,
				0.0f,
				"%.3f deg"
			);

			BeginPropertyRow("Lock Rotation");
			changed |= ImGui::UndoCheckbox("X##LockRotation", &collider.lockRotX);
			ImGui::SameLine();
			changed |= ImGui::UndoCheckbox("Y##LockRotation", &collider.lockRotY);
			ImGui::SameLine();
			changed |= ImGui::UndoCheckbox("Z##LockRotation", &collider.lockRotZ);

			ImGui::EndTable();
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNodeEx(
		"Physics Material",
		ImGuiTreeNodeFlags_DefaultOpen |
		ImGuiTreeNodeFlags_SpanAvailWidth
	)){
		if(BeginPropertyTable("ColliderMaterialProperties")){
			BeginPropertyRow("Static Friction");
			changed |= ImGui::UndoDragFloat(
				"##StaticFriction",
				&collider.staticFriction,
				0.01f,
				0.0f,
				1.0f
			);

			BeginPropertyRow("Dynamic Friction");
			changed |= ImGui::UndoDragFloat(
				"##DynamicFriction",
				&collider.dynamicFriction,
				0.01f,
				0.0f,
				1.0f
			);

			BeginPropertyRow("Restitution");
			changed |= ImGui::UndoDragFloat(
				"##Restitution",
				&collider.restitution,
				0.01f,
				0.0f,
				1.0f
			);

			ImGui::EndTable();
		}
		ImGui::TreePop();
	}

	collider.size.x = (std::max)(collider.size.x, 0.001f);
	collider.size.y = (std::max)(collider.size.y, 0.001f);
	collider.size.z = (std::max)(collider.size.z, 0.001f);
	collider.radius = (std::max)(collider.radius, 0.001f);
	collider.height = (std::max)(collider.height, 0.001f);
	collider.staticFriction = (std::clamp)(collider.staticFriction, 0.0f, 1.0f);
	collider.dynamicFriction = (std::clamp)(collider.dynamicFriction, 0.0f, 1.0f);
	collider.restitution = (std::clamp)(collider.restitution, 0.0f, 1.0f);
	component.needsUpdate |= changed;
}

inline void Inspect(ColliderComponent& component, SceneContext* context){
	ImGui::PushID(&component);
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 4.0f));

	DrawGeneralSettings(component);
	ImGui::Spacing();

	const float addButtonWidth = ImGui::GetContentRegionAvail().x;
	if(ImGui::Button("+ Add Collider", ImVec2(addButtonWidth, 0.0f))){
		ColliderShape shape;
		if(context && context->system){
			if(PhysicSystem* physics = context->system->GetSystem<PhysicSystem>()){
				if(!physics->GetLayers().empty()){
					shape.collisionLayer = physics->GetLayers().front().bit;
				}
			}
		}
		component.colliders.push_back(std::move(shape));
		component.needsUpdate = true;
	}

	if(component.colliders.empty()){
		ImGui::TextDisabled("No collider shapes.");
	}

	for(size_t index = 0; index < component.colliders.size();){
		ImGui::PushID(static_cast<int>(index));

		bool shapeOpen = false;
		bool removed = false;
		if(ImGui::BeginTable(
			"ColliderHeader",
			2,
			ImGuiTableFlags_SizingStretchProp |
			ImGuiTableFlags_NoSavedSettings
		)){
			ImGui::TableSetupColumn("Shape", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			const std::string label =
				"Collider " + std::to_string(index + 1);
			shapeOpen = ImGui::TreeNodeEx(
				label.c_str(),
				ImGuiTreeNodeFlags_DefaultOpen |
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_NoTreePushOnOpen
			);

			ImGui::TableSetColumnIndex(1);
			if(ImGui::SmallButton("Remove")){
				if(context && context->system){
					if(auto* physics = context->system->GetSystem<PhysicSystem>()){
						physics->ReleaseColliderShapeRuntime(&component, index);
					}
				}
				component.colliders.erase(component.colliders.begin() + index);
				component.needsUpdate = true;
				removed = true;
			}
			ImGui::EndTable();
		}

		if(shapeOpen && !removed){
			ImGui::Indent();
			DrawShape(component, component.colliders[index], context);
			ImGui::Unindent();
		}

		ImGui::Separator();
		ImGui::PopID();

		if(!removed){
			++index;
		}
	}

	ImGui::PopStyleVar();
	ImGui::PopID();
}

} // namespace ColliderComponentOperations

inline ColliderComponent::~ColliderComponent() = default;

inline void ColliderComponent::OnBeforeRemove(SceneContext* context){
	if(!context || !context->system) return;
	if(auto* physics = context->system->GetSystem<PhysicSystem>()){
		physics->ReleaseColliderRuntime(this);
	}
}

inline YAML::Node ColliderComponent::encode(){
	return ColliderComponentOperations::Encode(*this);
}

inline bool ColliderComponent::decode(SceneContext*, const YAML::Node& node){
	return ColliderComponentOperations::Decode(*this, node);
}

inline void ColliderComponent::inspector(SceneContext* context){
	ColliderComponentOperations::Inspect(*this, context);
}
