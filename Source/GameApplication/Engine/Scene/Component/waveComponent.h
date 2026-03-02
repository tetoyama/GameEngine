#pragma once
#include "Interface/IComponent.h"
#include "BackEnds/YAMLConverters.h"
#include "Backends/myVector2.h"
#include "meshRendererComponent.h"

class WaveComponent: public IComponent {
public:
	int Resolution = 50;         // グリッド解像度（途中変更可）
	float Amplitude = 0.2f;      // 波の振幅
	float Wavelength = 2.0f;     // 波長
	float Speed = 1.0f;          // 波の進行速度
	float Time = 0.0f;           // 経過時間
	int CurrentResolution = -1;  // 現在のメッシュ状態
	MeshRendererComponent* meshRenderer = nullptr;

	// 水面反射パラメータ
	bool  ReflectionEnabled     = true;   // 水面反射を有効にするか
	float ReflectionStrength    = 0.5f;   // 反射強度 (0=なし, 1=完全)
	float DistortionStrength    = 0.02f;  // 波による歪み強度

	~WaveComponent(){
		if(meshRenderer){
			delete meshRenderer;
			meshRenderer = nullptr;
		}
	}

	YAML::Node encode() override{
		YAML::Node node;
		node["Resolution"] = Resolution;
		node["Amplitude"] = Amplitude;
		node["Wavelength"] = Wavelength;
		node["Speed"] = Speed;
		node["ReflectionEnabled"]  = ReflectionEnabled;
		node["ReflectionStrength"] = ReflectionStrength;
		node["DistortionStrength"] = DistortionStrength;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(node["Resolution"]) Resolution = node["Resolution"].as<int>();
		if(node["Amplitude"]) Amplitude = node["Amplitude"].as<float>();
		if(node["Wavelength"]) Wavelength = node["Wavelength"].as<float>();
		if(node["Speed"]) Speed = node["Speed"].as<float>();
		if(node["ReflectionEnabled"])  ReflectionEnabled  = node["ReflectionEnabled"].as<bool>();
		if(node["ReflectionStrength"]) ReflectionStrength = node["ReflectionStrength"].as<float>();
		if(node["DistortionStrength"]) DistortionStrength = node["DistortionStrength"].as<float>();
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text("Wave Component");
		ImGui::Separator();
		ImGui::DragInt("Resolution", &Resolution, 1, 5, 200);
		ImGui::DragFloat("Amplitude", &Amplitude, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("Wavelength", &Wavelength, 0.01f, 0.1f, 10.0f);
		ImGui::DragFloat("Speed", &Speed, 0.01f, 0.0f, 10.0f);

		if(ImGui::Button("Rebuild Mesh")){
			CurrentResolution = -1; // 再構築要求
		}

		ImGui::Text("Current Time: %.2f", Time);

		ImGui::Separator();
		ImGui::Text("Water Reflection");
		ImGui::Checkbox("Reflection Enabled", &ReflectionEnabled);
		if(ReflectionEnabled){
			ImGui::DragFloat("Reflection Strength", &ReflectionStrength, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Distortion Strength", &DistortionStrength, 0.001f, 0.0f, 0.2f);
		}
	}
};
