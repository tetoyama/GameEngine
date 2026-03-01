#pragma once
#include "Interface/IComponent.h"
#include "BackEnds/YAMLConverters.h"
#include "Shader/Common.hlsl"
#include "Shader/CommonDefine.h"

class LightComponent: public IComponent {
public:
	float moveSpeed = 5.0f;
	LIGHT light;

	LightComponent(){
		light.Enable = true;
		light.LightType = LIGHT_TYPE_POINT;
		light.CastShadow = true;
		light.Ambient = float4(0, 0, 0, 0);
		light.Diffuse = float4(1, 1, 1, 1);
		light.Param = float4(10, 0, 0, 0);
		light.ShadowBias = float4(0.001f, 0.003f, 0.0f, 0.0f);
	}

	YAML::Node encode() override{
		YAML::Node node;


		node["Enable"] = light.Enable;
		node["LightType"] = light.LightType;
		node["CastShadow"] = light.CastShadow;

		node["Position"] = light.Position;
		node["Direction"] = light.Direction;

		node["Diffuse"] = light.Diffuse;
		node["Ambient"] = light.Ambient;

		node["Param"] = light.Param;
		node["ShadowBias"] = light.ShadowBias;

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

		if(node["CastShadow"])
			light.CastShadow = node["CastShadow"].as<BOOL>();

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

		if(node["ShadowBias"])
			light.ShadowBias = node["ShadowBias"].as<DirectX::XMFLOAT4>();

		if(node["LightView"])
			light.LightView = node["LightView"].as<DirectX::XMFLOAT4X4>();

		if(node["LightProjection"])
			light.LightProjection = node["LightProjection"].as<DirectX::XMFLOAT4X4>();

		return true;
	}

	void inspector(SceneContext* context){
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

		// ライト有効・無効
		ImGui::Checkbox("Enable", (bool*)&light.Enable);
		ImGui::Checkbox("CastShadow", (bool*)&light.CastShadow);

		// ライトの種類
		const char* lightTypes[] = {"None","Directional", "Point", "Spot", "DirectionalCSM"};
		int selected = static_cast<int>(light.LightType);
		if(ImGui::Combo("Light Type", &selected, lightTypes, IM_ARRAYSIZE(lightTypes))){
			light.LightType = selected;
		}

		// 色設定
		ImGui::ColorEdit4("Diffuse", reinterpret_cast<float*>(&light.Diffuse));
		ImGui::ColorEdit4("Ambient", reinterpret_cast<float*>(&light.Ambient));

		// パラメータ (例: Point Light の範囲、減衰など)
		ImGui::DragFloat4("Param", reinterpret_cast<float*>(&light.Param), 0.1f);

		// シャドウバイアス (x: 定数バイアス, y: スロープスケールバイアス)
		if (light.CastShadow)
		{
			ImGui::DragFloat("Shadow Constant Bias", &light.ShadowBias.x, 0.0001f, 0.0f, 0.1f, "%.5f");
			ImGui::DragFloat("Shadow Slope Bias",    &light.ShadowBias.y, 0.0001f, 0.0f, 0.1f, "%.5f");
		}

		ImGui::PopStyleVar();
	}
};
