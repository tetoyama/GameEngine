#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"
#include "Service/YAMLConverters.h"

class TransformComponent : public IComponent{
public:
	Vector3 position = Vector3(0, 0, 0);
	Vector3 rotation = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);

	YAML::Node encode() override{
		YAML::Node node;
		node["Component"] = "TransformComponent";
		node["Position"] = position;
		node["Rotation"] = rotation;
		node["Scale"] = scale;

		return node;
	}

	bool decode(const YAML::Node& node) override {
		if (!node.IsMap()) { return false; }

		if (node["Position"]) {
			position = node["Position"].as<Vector3>();
		}
		if (node["Rotation"]) {
			rotation = node["Rotation"].as<Vector3>();
		}
		if (node["Scale"]) {
			scale = node["Scale"].as<Vector3>();
		}
		return true;
	}

	void inspector(SceneContext* context) override {
		// Position
		ImGui::Text("Position");
		ImGui::SameLine(100);
		ImGui::DragFloat3("##Position", &position.x, 0.1f);
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip("Move the object position");

		// Rotation
		ImGui::Text("Rotation");
		ImGui::SameLine(100);
		ImGui::DragFloat3("##Rotation", &rotation.x, 0.01f, -DirectX::XM_PI, DirectX::XM_PI);
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip("Rotate the object (in radian)");

		// Scale
		ImGui::Text("Scale");
		ImGui::SameLine(100);
		ImGui::DragFloat3("##Scale", &scale.x, 0.01f, 0.01f, 10.0f);
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip("Scale the object");

	}

	Vector3 front() const {
		float pitch = rotation.x;
		float yaw = rotation.y;
		float roll = rotation.z;

		float cosPitch = cos(pitch);
		float sinPitch = sin(pitch);
		float cosYaw = cos(yaw);
		float sinYaw = sin(yaw);
		float cosRoll = cos(roll);
		float sinRoll = sin(roll);

		return Vector3(
			cosPitch * sinYaw,
			sinPitch,
			cosPitch * cosYaw
		);
	}

	Vector3 up() const {
		float pitch = rotation.x;
		float yaw = rotation.y;
		float roll = rotation.z;

		float cosPitch = cos(pitch);
		float sinPitch = sin(pitch);
		float cosYaw = cos(yaw);
		float sinYaw = sin(yaw);
		float cosRoll = cos(roll);
		float sinRoll = sin(roll);

		return Vector3(
			-cosPitch * sinYaw * sinRoll + sinPitch * cosRoll,
			cosPitch * cosRoll + sinPitch * sinYaw * sinRoll,
			cosYaw * sinPitch
		);
	}

	Vector3 right() const {
		float pitch = rotation.x;
		float yaw = rotation.y;
		float roll = rotation.z;

		float cosPitch = cos(pitch);
		float sinPitch = sin(pitch);
		float cosYaw = cos(yaw);
		float sinYaw = sin(yaw);
		float cosRoll = cos(roll);
		float sinRoll = sin(roll);

		return Vector3(
			cosPitch * cosYaw,
			-sinPitch,
			cosPitch * sinYaw
		);
	}
};
