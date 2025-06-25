#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

#include "Backends/myVector3.h"
#include <DirectXMath.h>

class CameraComponent : public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		node["Component"] = "CameraComponent";

		node["isLock"] = isLock;
		node["Target"] = Target;
		node["NearClip"] = NearClip;
		node["FarClip"] = FarClip;
		node["FOV"] = FOV;
		node["viewMatrix"] = viewMatrix;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text("NearClip");
		ImGui::SameLine(120);
		ImGui::DragFloat("##NearClip", &NearClip, 0.01f,0.01f, FarClip - 0.01f);
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip("NearClip");

		ImGui::Text("FarClip");
		ImGui::SameLine(120);
		ImGui::DragFloat("##FarClip", &FarClip, 0.01f, NearClip + 0.01f, 1024.0f);
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip("FarClip");

		if(FarClip <= NearClip)
			FarClip = NearClip + 0.01f;

		ImGui::Text("FOV");
		ImGui::SameLine(120);
		ImGui::DragFloat("##FOV", &FOV, 0.01f);
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip("FOV");
	}

	bool isLock = false;
	Vector3 Target = Vector3(0.0f,0.0f,0.0f);

	float NearClip = 0.01f;
	float FarClip = 256.0f;

	float FOV = 1.0f;

	DirectX::XMMATRIX viewMatrix{};
};
