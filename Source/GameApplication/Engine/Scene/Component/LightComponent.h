#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"
#include "Shader/CommonBuffer.h"

class LightComponent: public IComponent {
public:
	float moveSpeed = 5.0f;
	LIGHT light;

	LightComponent(){
		light.Enable = true;
		light.LightType = LIGHT_TYPE_DIRECTIONAL;
		light.CastShadow = true;
	}

	YAML::Node encode() override{
		YAML::Node node;


		node["Enable"] = light.Enable;
		node["LightType"] = light.LightType;

		node["Position"] = light.Position;
		node["Direction"] = light.Direction;

		node["Diffuse"] = light.Diffuse;
		node["Ambient"] = light.Ambient;

		node["Param"] = light.Param;

		node["LightView"] = light.LightView;
		node["LightProjection"] = light.LightProjection;

		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(!node.IsMap()){
			return false;
		}

		if(node["Enable"])
			light.Enable = node["Enable"].as<BOOL>();

		if(node["LightType"])
			light.LightType = node["LightType"].as<UINT>();

		if(node["Direction"])
			light.Direction = node["Direction"].as<DirectX::XMFLOAT4>();

		if(node["Diffuse"])
			light.Diffuse = node["Diffuse"].as<DirectX::XMFLOAT4>();

		if(node["Ambient"])
			light.Ambient = node["Ambient"].as<DirectX::XMFLOAT4>();

		if(node["Position"])
			light.Position = node["Position"].as<DirectX::XMFLOAT4>();

		if(node["Param"])
			light.Param = node["Param"].as<DirectX::XMFLOAT4>();

		if(node["LightView"])
			light.LightView = node["LightView"].as<DirectX::XMMATRIX>();

		if(node["LightProjection"])
			light.LightProjection = node["LightProjection"].as<DirectX::XMMATRIX>();

		return true;
	}

	void inspector(SceneContext* context){
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

		// ライト有効・無効
		ImGui::Checkbox("Enable", (bool*)&light.Enable);
		ImGui::Checkbox("CastShadow", (bool*)&light.Enable);

		// ライトの種類
		const char* lightTypes[] = {"Directional", "Point", "Spot"};
		int selected = static_cast<int>(light.LightType);
		if(ImGui::Combo("Light Type", &selected, lightTypes, IM_ARRAYSIZE(lightTypes))){
			light.LightType = selected;
		}

		// 色設定
		ImGui::ColorEdit4("Diffuse", reinterpret_cast<float*>(&light.Diffuse));
		ImGui::ColorEdit4("Ambient", reinterpret_cast<float*>(&light.Ambient));
		//ImGui::ColorEdit4("Sky Color", reinterpret_cast<float*>(&light.SkyColor));
		//ImGui::ColorEdit4("Ground Color", reinterpret_cast<float*>(&light.GroundColor));

		// 各種ベクトル設定
		//ImGui::DragFloat3("Angle", reinterpret_cast<float*>(&light.Angle), 0.1f);

		// パラメータ (例: Point Light の範囲、減衰など)
		ImGui::DragFloat4("Param", reinterpret_cast<float*>(&light.Param), 0.1f);

		// 地面の法線方向
		//ImGui::DragFloat3("GroundNormal", reinterpret_cast<float*>(&light.GroundNormal), 0.1f);

		ImGui::PopStyleVar();
	}
};
                       