// =======================================================================
//
// ColliderComponentOperations.h
//
// ColliderComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include <algorithm>
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

	if(node["isDynamic"]) component.isDynamic = node["isDynamic"].as<bool>();
	if(node["autoMass"]) component.autoMass = node["autoMass"].as<bool>();
	if(node["Mass"]) component.Mass = node["Mass"].as<float>();

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
			if(colliderNode["radius"]) collider.radius = colliderNode["radius"].as<float>();
			if(colliderNode["height"]) collider.height = colliderNode["height"].as<float>();
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
			if(colliderNode["lockRotX"]) collider.lockRotX = colliderNode["lockRotX"].as<bool>();
			if(colliderNode["lockRotY"]) collider.lockRotY = colliderNode["lockRotY"].as<bool>();
			if(colliderNode["lockRotZ"]) collider.lockRotZ = colliderNode["lockRotZ"].as<bool>();
			if(colliderNode["isTrigger"]){
				collider.isTrigger = colliderNode["isTrigger"].as<bool>();
			}
			if(colliderNode["boneName"]){
				collider.boneName = colliderNode["boneName"].as<std::string>();
			}

			collider.radius = (std::max)(collider.radius, 0.001f);
			collider.height = (std::max)(collider.height, 0.001f);
			collider.staticFriction = (std::clamp)(collider.staticFriction, 0.0f, 1.0f);
			collider.dynamicFriction = (std::clamp)(collider.dynamicFriction, 0.0f, 1.0f);
			collider.restitution = (std::clamp)(collider.restitution, 0.0f, 1.0f);
			component.colliders.push_back(std::move(collider));
		}
	}

	component.needsUpdate = true;
	return true;
}

template<typename TActor>
inline void ReleaseActor(TActor*& actor){
	if(!actor){
		return;
	}
	if(actor->userData){
		delete static_cast<ActorEntityInfo*>(actor->userData);
		actor->userData = nullptr;
	}
	actor->release();
	actor = nullptr;
}

inline void ReleaseShapeRuntime(ColliderShape& collider){
	if(collider.pxShape){
		collider.pxShape->release();
		collider.pxShape = nullptr;
	}
	if(collider.pxMaterial){
		collider.pxMaterial->release();
		collider.pxMaterial = nullptr;
	}
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

inline void DrawGeneralSettings(ColliderComponent& component){
	bool changed = false;
	changed |= ImGui::UndoCheckbox("isDynamic", &component.isDynamic);
	changed |= ImGui::UndoCheckbox("autoMass", &component.autoMass);
	if(component.isDynamic && !component.autoMass){
		changed |= ImGui::UndoDragFloat("Mass", &component.Mass, 0.1f, 0.001f);
	}
	component.needsUpdate |= changed;
}

inline void DrawCollisionLayer(
	ColliderShape& collider,
	SceneContext* context,
	float labelPosition,
	bool& changed
){
	PhysicSystem* physics = nullptr;
	if(context && context->system){
		physics = context->system->GetSystem<PhysicSystem>();
	}

	ImGui::TextUnformatted("Collision Layer");
	ImGui::SameLine(labelPosition);

	if(!physics || physics->GetLayers().empty()){
		ImGui::TextDisabled("(no layers)");
		return;
	}

	const auto& layers = physics->GetLayers();
	int currentIndex = 0;
	for(int index = 0; index < static_cast<int>(layers.size()); ++index){
		if(collider.collisionLayer == layers[index].bit){
			currentIndex = index;
			break;
		}
	}

	if(ImGui::BeginCombo("##CollisionLayer", layers[currentIndex].name.c_str())){
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
}

inline void DrawBoneName(
	ColliderComponent& component,
	ColliderShape& collider,
	SceneContext* context,
	float labelPosition,
	bool& changed
){
	ImGui::TextUnformatted("Bone Name");
	ImGui::SameLine(labelPosition);

	ModelRendererComponent* modelRenderer = FindModelRenderer(component, context);
	const char* preview = collider.boneName.empty()
		? "(none)"
		: collider.boneName.c_str();

	if(modelRenderer && modelRenderer->model &&
		!modelRenderer->model->m_BoneIndexMap.empty()){
		if(ImGui::BeginCombo("##BoneName", preview)){
			std::vector<std::string> boneNames{std::string{}};
			for(const auto& [name, boneIndex] : modelRenderer->model->m_BoneIndexMap){
				(void)boneIndex;
				boneNames.push_back(name);
			}
			std::sort(boneNames.begin() + 1, boneNames.end());
			for(const std::string& boneName : boneNames){
				const bool selected = collider.boneName == boneName;
				const char* label = boneName.empty() ? "(none)" : boneName.c_str();
				if(ImGui::Selectable(label, selected)){
					collider.boneName = boneName;
					changed = true;
				}
				if(selected){
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		return;
	}

	char buffer[128]{};
	std::strncpy(buffer, collider.boneName.c_str(), sizeof(buffer) - 1);
	if(ImGui::InputText("##BoneName", buffer, sizeof(buffer))){
		collider.boneName = buffer;
		changed = true;
	}
}

inline void DrawShape(
	ColliderComponent& component,
	ColliderShape& collider,
	SceneContext* context,
	size_t index
){
	constexpr float labelPosition = 120.0f;
	bool changed = false;
	const std::string suffix = std::to_string(index);

	if(ImGui::TreeNodeEx("Param", ImGuiTreeNodeFlags_DefaultOpen)){
		int type = static_cast<int>(collider.type);
		ImGui::TextUnformatted("Type");
		ImGui::SameLine(labelPosition);
		if(ImGui::Combo(
			("##Type" + suffix).c_str(),
			&type,
			"Box\0Sphere\0Capsule\0Mesh\0HeightMap\0"
		)){
			collider.type = static_cast<ColliderType>(type);
			changed = true;
		}

		ImGui::TextUnformatted("Is Trigger");
		ImGui::SameLine(labelPosition);
		changed |= ImGui::UndoCheckbox(
			("##IsTrigger" + suffix).c_str(),
			&collider.isTrigger
		);

		DrawCollisionLayer(collider, context, labelPosition, changed);

		if(collider.type == ColliderType::Box){
			ImGui::TextUnformatted("Size");
			ImGui::SameLine(labelPosition);
			changed |= ImGui::UndoDragFloat3(
				("##Size" + suffix).c_str(),
				&collider.size.x,
				0.1f
			);
		} else if(collider.type == ColliderType::Sphere){
			ImGui::TextUnformatted("Radius");
			ImGui::SameLine(labelPosition);
			changed |= ImGui::UndoDragFloat(
				("##Radius" + suffix).c_str(),
				&collider.radius,
				0.1f,
				0.001f
			);
		} else if(collider.type == ColliderType::Capsule){
			ImGui::TextUnformatted("Radius");
			ImGui::SameLine(labelPosition);
			changed |= ImGui::UndoDragFloat(
				("##Radius" + suffix).c_str(),
				&collider.radius,
				0.1f,
				0.001f
			);
			ImGui::TextUnformatted("Height");
			ImGui::SameLine(labelPosition);
			changed |= ImGui::UndoDragFloat(
				("##Height" + suffix).c_str(),
				&collider.height,
				0.1f,
				0.001f
			);
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNodeEx("Offset", ImGuiTreeNodeFlags_DefaultOpen)){
		DrawBoneName(component, collider, context, labelPosition, changed);

		ImGui::TextUnformatted("Position");
		ImGui::SameLine(labelPosition);
		changed |= ImGui::UndoDragFloat3(
			("##Position" + suffix).c_str(),
			&collider.offset.x,
			0.1f
		);

		ImGui::TextUnformatted("Rotation");
		ImGui::SameLine(labelPosition);
		changed |= ImGui::UndoDragFloat3(
			("##Rotation" + suffix).c_str(),
			&collider.rotationOffset.x,
			1.0f
		);

		ImGui::TextUnformatted("Lock Rotation");
		ImGui::SameLine(labelPosition);
		changed |= ImGui::UndoCheckbox(("X##LockRot" + suffix).c_str(), &collider.lockRotX);
		ImGui::SameLine();
		changed |= ImGui::UndoCheckbox(("Y##LockRot" + suffix).c_str(), &collider.lockRotY);
		ImGui::SameLine();
		changed |= ImGui::UndoCheckbox(("Z##LockRot" + suffix).c_str(), &collider.lockRotZ);
		ImGui::TreePop();
	}

	if(ImGui::TreeNodeEx("Friction", ImGuiTreeNodeFlags_DefaultOpen)){
		ImGui::TextUnformatted("Static");
		ImGui::SameLine(labelPosition);
		changed |= ImGui::UndoDragFloat(
			("##Static" + suffix).c_str(),
			&collider.staticFriction,
			0.01f,
			0.0f,
			1.0f
		);

		ImGui::TextUnformatted("Dynamic");
		ImGui::SameLine(labelPosition);
		changed |= ImGui::UndoDragFloat(
			("##Dynamic" + suffix).c_str(),
			&collider.dynamicFriction,
			0.01f,
			0.0f,
			1.0f
		);

		ImGui::TextUnformatted("Restitution");
		ImGui::SameLine(labelPosition);
		changed |= ImGui::UndoDragFloat(
			("##Restitution" + suffix).c_str(),
			&collider.restitution,
			0.01f,
			0.0f,
			1.0f
		);
		ImGui::TreePop();
	}

	component.needsUpdate |= changed;
}

inline void Inspect(ColliderComponent& component, SceneContext* context){
	ImGui::PushID(&component);
	ImGui::TextUnformatted("Collider Component");
	DrawGeneralSettings(component);
	ImGui::Separator();

	const std::string treeLabel =
		"Colliders(" + std::to_string(component.colliders.size()) + ")";
	const bool open = ImGui::TreeNodeEx(
		treeLabel.c_str(),
		ImGuiTreeNodeFlags_DefaultOpen
	);
	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 85.0f);
	if(ImGui::Button("Add Collider")){
		component.colliders.emplace_back();
		component.needsUpdate = true;
	}

	if(open){
		for(size_t index = 0; index < component.colliders.size();){
			ImGui::PushID(static_cast<int>(index));
			const std::string label = "Collider" + std::to_string(index);
			const bool shapeOpen = ImGui::TreeNodeEx(
				label.c_str(),
				ImGuiTreeNodeFlags_DefaultOpen
			);
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 60.0f);

			bool removed = false;
			if(ImGui::SmallButton("Remove")){
				ReleaseShapeRuntime(component.colliders[index]);
				component.colliders.erase(component.colliders.begin() + index);
				component.needsUpdate = true;
				removed = true;
			}

			if(shapeOpen){
				if(!removed){
					DrawShape(component, component.colliders[index], context, index);
				}
				ImGui::TreePop();
			}

			ImGui::PopID();
			ImGui::Separator();
			if(!removed){
				++index;
			}
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
}

} // namespace ColliderComponentOperations

inline ColliderComponent::~ColliderComponent(){
	ColliderComponentOperations::ReleaseActor(pRigidbodyStatic);
	ColliderComponentOperations::ReleaseActor(pRigidbodyDynamic);
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
