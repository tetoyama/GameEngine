#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

class PlayerComponent : public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;

		node["moveSpeed"] = moveSpeed;
		node["cameraRotate"] = cameraRotate;
		node["cameraDistance"] = cameraDistance;
		node["cameraLerp"] = cameraLerp;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		if (!node.IsMap()) { return false; }

		if (node["moveSpeed"]) {
			moveSpeed = node["moveSpeed"].as<float>();
		}
		if (node["cameraRotate"]) {
			cameraRotate = node["cameraRotate"].as<float>();
		}
		if (node["cameraDistance"]) {
			cameraDistance = node["cameraDistance"].as<float>();
		}
		if (node["cameraLerp"]) {
			cameraLerp = node["cameraLerp"].as<float>();
		}
		return true;
	}

	void inspector(SceneContext* context) override{

		ImGui::Text("MoveSpeed");
		ImGui::SameLine(100);
		ImGui::DragFloat("##moveSpeed", &moveSpeed);
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip("moveSpeed");

		ImGui::Text("cmrRotate");
		ImGui::SameLine(100);
		ImGui::DragFloat("##cameraRotate", &cameraRotate);
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip("cameraRotate");

		ImGui::Text("cmrDistance");
		ImGui::SameLine(100);
		ImGui::DragFloat("##cameraDistance", &cameraDistance);
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip("cameraDistance");

		ImGui::Text("cmrLerp");
		ImGui::SameLine(100);
		ImGui::DragFloat("##cameraLerp", &cameraLerp);
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip("cameraLerp");
	}

	float moveSpeed = 5.0f;
	float cameraRotate = 2.0f;
	float cameraDistance = 20.0f;
	float cameraLerp = 8.0f;
};
