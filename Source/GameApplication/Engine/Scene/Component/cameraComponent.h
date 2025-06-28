#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

#include "Backends/myVector3.h"
#include <DirectXMath.h>

class CameraComponent : public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;

		node["isLock"] = isLock;
		node["Target"] = Target;
		node["NearClip"] = NearClip;
		node["FarClip"] = FarClip;
		node["FOV"] = FOV;
		node["viewMatrix"] = viewMatrix;
		return node;
	}

	bool decode(const YAML::Node& node) override{

		if(node["isLock"]){
			isLock = node["isLock"].as<bool>();
		}
		if(node["Target"]){
			Target = node["Target"].as<Vector3>();
		}
		if(node["NearClip"]){
			NearClip = node["NearClip"].as<float>();
		}
		if(node["FarClip"]){
			FarClip = node["FarClip"].as<float>();
		}
		if(node["FOV"]){
			FOV = node["FOV"].as<float>();
		}
		if(node["viewMatrix"]){
			viewMatrix = node["viewMatrix"].as<DirectX::XMMATRIX>();
		}
		return true;
	}

	void inspector(SceneContext* context) override{

		ImGui::Text("NearClip");
		ImGui::SameLine(100);
		ImGui::DragFloat("##NearClip", &NearClip, 0.01f,0.01f, FarClip - 0.01f);
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip("NearClip");

		ImGui::Text("FarClip");
		ImGui::SameLine(100);
		ImGui::DragFloat("##FarClip", &FarClip, 0.01f, NearClip + 0.01f, 1024.0f);
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip("FarClip");

		if (NearClip <= 0.0f)
			NearClip = 0.01f;

		if(FarClip <= NearClip)
			FarClip = NearClip + 0.01f;

		ImGui::Text("FOV");
		ImGui::SameLine(100);
		ImGui::DragFloat("##FOV", &FOV, 0.01f, 0.01f);
		if(FOV <= 0.0f)
			FOV = 0.01f;

		if(ImGui::IsItemHovered())
			ImGui::SetTooltip("FOV");

		ImGui::Text("isLock");
		ImGui::SameLine(100);
		if(ImGui::Button(isLock ? "On" : "Off")){
			isLock = !isLock;
		}

		if(isLock){
			ImGui::Text("Target Position");
			ImGui::SameLine(100);
			ImGui::DragFloat3("##Target", &Target.x, 0.01f);
			if(ImGui::IsItemHovered())
				ImGui::SetTooltip("Target Position");
		}
		else{
			ImGui::Text("Target Position is not Locked");
		}
	}

	bool isLock = false;
	Vector3 Target = Vector3(0.0f,0.0f,0.0f);

	float NearClip = 0.01f;
	float FarClip = 256.0f;

	float FOV = 1.0f;

	DirectX::XMMATRIX viewMatrix{};
};
