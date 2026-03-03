#pragma once
#include "Interface/IComponent.h"
#include "BackEnds/YAMLConverters.h"
#include "Backends/myVector2.h"
#include "meshRendererComponent.h"

class WaveComponent: public IComponent {
public:
	int Resolution = 50;           // グリッド解像度（途中変更可）
	float Amplitude = 0.2f;        // 波の振幅
	float Wavelength = 2.0f;       // 波長
	float Speed = 1.0f;            // 波の進行速度
	float Steepness = 0.3f;        // Gerstner波のスティープネス (0=サイン波, 1=急峻)
	float Time = 0.0f;             // 経過時間
	int CurrentResolution = -1;    // 現在のメッシュ状態
	MeshRendererComponent* meshRenderer = nullptr;

	// 水面外観パラメータ (WaveGBufferPS へ渡す)
	float NormalIntensity = 0.5f;  // 法線アニメーション強度
	float WaterRoughness  = 0.05f; // 水面ラフネス (低いほど鏡面に近い)
	float WaterMetallic   = 0.9f;  // 水面金属度  (高いほど反射が強い)

	~WaveComponent(){
		if(meshRenderer){
			delete meshRenderer;
			meshRenderer = nullptr;
		}
	}

	YAML::Node encode() override{
		YAML::Node node;
		node["Resolution"]     = Resolution;
		node["Amplitude"]      = Amplitude;
		node["Wavelength"]     = Wavelength;
		node["Speed"]          = Speed;
		node["Steepness"]      = Steepness;
		node["NormalIntensity"]= NormalIntensity;
		node["WaterRoughness"] = WaterRoughness;
		node["WaterMetallic"]  = WaterMetallic;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(node["Resolution"])      Resolution      = node["Resolution"].as<int>();
		if(node["Amplitude"])       Amplitude       = node["Amplitude"].as<float>();
		if(node["Wavelength"])      Wavelength      = node["Wavelength"].as<float>();
		if(node["Speed"])           Speed           = node["Speed"].as<float>();
		if(node["Steepness"])       Steepness       = node["Steepness"].as<float>();
		if(node["NormalIntensity"]) NormalIntensity = node["NormalIntensity"].as<float>();
		if(node["WaterRoughness"])  WaterRoughness  = node["WaterRoughness"].as<float>();
		if(node["WaterMetallic"])   WaterMetallic   = node["WaterMetallic"].as<float>();
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text("Wave Component");
		ImGui::Separator();
		ImGui::Text("-- Mesh --");
		ImGui::UndoDragInt("Resolution", &Resolution, 1, 5, 200);
		if(ImGui::Button("Rebuild Mesh")){
			CurrentResolution = -1;
		}

		ImGui::Separator();
		ImGui::Text("-- Wave Physics --");
		ImGui::UndoDragFloat("Amplitude",  &Amplitude,  0.01f, 0.0f, 5.0f);
		ImGui::UndoDragFloat("Wavelength", &Wavelength, 0.01f, 0.1f, 10.0f);
		ImGui::UndoDragFloat("Speed",      &Speed,      0.01f, 0.0f, 10.0f);
		ImGui::UndoDragFloat("Steepness",  &Steepness,  0.01f, 0.0f, 1.0f);

		ImGui::Separator();
		ImGui::Text("-- Water Appearance --");
		ImGui::UndoDragFloat("NormalIntensity", &NormalIntensity, 0.01f, 0.0f,  1.0f);
		ImGui::UndoDragFloat("WaterRoughness",  &WaterRoughness,  0.01f, 0.0f,  1.0f);
		ImGui::UndoDragFloat("WaterMetallic",   &WaterMetallic,   0.01f, 0.0f,  1.0f);

		ImGui::Separator();
		ImGui::Text("Current Time: %.2f", Time);
	}
};
