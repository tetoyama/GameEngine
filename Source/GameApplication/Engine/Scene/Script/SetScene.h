#pragma once
#include "Component/CustomScriptComponent.h"

class SetScene: public CustomScriptComponent {
public:

	SetScene(): CustomScriptComponent("SetScene"){}

	virtual YAML::Node encode() override{
		YAML::Node node;
		node["ScriptName"] = scriptName;
		node["SceneName"] = sceneName;
		return node;
	}
	virtual bool decode(const YAML::Node& node) override{
		if(node["ScriptName"])
			scriptName = node["ScriptName"].as<std::string>();
		if(node["SceneName"])
			sceneName = node["SceneName"].as<std::string>();
		return true;
	}
	virtual void inspector(SceneContext* context) override{
		ImGui::Text(scriptName.c_str());
		ImGui::InputText("Scene Name", &sceneName[0], sceneName.capacity() + 1);
		ImGui::DragInt("Test", &Temp);
	}

	void OnStart() override{

	}
	void OnUpdate(float dt) override{
		OutputDebugStringA(("SetScene Update\n"));
		auto* comp = GetComponent<CustomScriptComponent>();
		Temp++;
		if(GetKeyDown(VK_RETURN)){
			LoadScene(sceneName);
		}
	}

	virtual void OnFixedUpdate(float dt)override{}
	virtual void OnDraw() override{}
	virtual void OnEditorUpdate(float dt)override{}
	virtual void OnStop() override{}
private:
	int Temp = 0;
	std::string sceneName = "Asset/Scene/Scene.yaml"; // デフォルトのシーン名
};
