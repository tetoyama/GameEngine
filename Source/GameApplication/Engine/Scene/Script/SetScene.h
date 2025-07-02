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
		if(ImGui::BeginDragDropTarget()){
			if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
				const char* droppedPath = (const char*)payload->Data;
				std::string _Path = std::string(droppedPath);

				sceneName = _Path;
			}
			ImGui::EndDragDropTarget();
		}
	}

	void OnStart() override{

	}
	void OnUpdate(float dt) override{
		auto* comp = GetComponent<CustomScriptComponent>();
		if(GetKeyDown(VK_RETURN)){
			LoadScene(sceneName);
		}
	}

	virtual void OnFixedUpdate(float dt)override{}
	virtual void OnDraw() override{}
	virtual void OnEditorUpdate(float dt)override{}
	virtual void OnStop() override{}
private:
	std::string sceneName = "Asset/Scene/scene.yaml"; // デフォルトのシーン名
};
