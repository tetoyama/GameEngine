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

	bool UseEnvironmentMap = false; // 環境マッピングを使用するか

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
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(node["Resolution"]) Resolution = node["Resolution"].as<int>();
		if(node["Amplitude"]) Amplitude = node["Amplitude"].as<float>();
		if(node["Wavelength"]) Wavelength = node["Wavelength"].as<float>();
		if(node["Speed"]) Speed = node["Speed"].as<float>();
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text("Wave Component");
		ImGui::Separator();
		ImGui::UndoDragInt("Resolution", &Resolution, 1, 5, 200);
		ImGui::UndoDragFloat("Amplitude", &Amplitude, 0.01f, 0.0f, 5.0f);
		ImGui::UndoDragFloat("Wavelength", &Wavelength, 0.01f, 0.1f, 10.0f);
		ImGui::UndoDragFloat("Speed", &Speed, 0.01f, 0.0f, 10.0f);

		if(ImGui::Button("Rebuild Mesh")){
			CurrentResolution = -1; // 再構築要求
		}

		ImGui::Separator();

		ImGui::Text("Current Time: %.2f", Time);
	}
};
