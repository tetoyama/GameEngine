#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

class ScoreManager: public CustomScriptComponent {
public:

	ScoreManager(): CustomScriptComponent("ScoreManager"){}

	YAML::Node encode() override{
		YAML::Node node;
		node["ScriptName"] = scriptName;
		node["Score"] = Score;
		return node;
	}
	bool decode(const YAML::Node& node) override{
		if(node["ScriptName"])
			scriptName = node["ScriptName"].as<std::string>();
		if(node["Score"])
			Score = node["Score"].as<int>();
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text(scriptName.c_str());
		ImGui::InputInt("Score", &Score);
	}

	void OnStart() override{}
	void OnUpdate(float dt) override{}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}

	int Score = 0;
private:
};
