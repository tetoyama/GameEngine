#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

class PlayerController: public CustomScriptComponent {
public:

	PlayerController(): CustomScriptComponent("PlayerController"){}

	YAML::Node encode() override{
		YAML::Node node;
		node["ScriptName"] = scriptName;
		return node;
	}
	bool decode(const YAML::Node& node) override{
		if(node["ScriptName"])
			scriptName = node["ScriptName"].as<std::string>();

		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text(scriptName.c_str());
	}

	void OnStart() override{}
	void OnUpdate(float dt) override{

	}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}

private:
};
