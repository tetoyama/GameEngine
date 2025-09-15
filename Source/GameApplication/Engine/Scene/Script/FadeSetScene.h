#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

class FadeSetScene: public CustomScriptComponent {
public:

	FadeSetScene(): CustomScriptComponent("FadeSetScene"){}
	FadeOutSprite* fade = nullptr;
	YAML::Node encode() override{
		YAML::Node node;
		node["ScriptName"] = scriptName;
		node["SceneName"] = sceneName;
		return node;
	}
	bool decode(const YAML::Node& node) override{
		if(node["ScriptName"])
			scriptName = node["ScriptName"].as<std::string>();
		if(node["SceneName"])
			sceneName = node["SceneName"].as<std::string>();
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text(scriptName.c_str());
		ImGui::InputText("Scene Name", &sceneName[0], sceneName.capacity() + 1);
		if(ImGui::BeginDragDropTarget()){
			if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
				const char* droppedPath = (const char*)payload->Data;
				std::string _Path = std::string(droppedPath);
				if(HasExtension(_Path, ".yaml")){
					sceneName = _Path;
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	void OnStart() override{
		auto fadeEntities = m_context->component->FindEntitiesWithComponent<FadeOutSprite>();
		if(!fadeEntities.empty()){
			fade = m_context->component->GetComponent<FadeOutSprite>(fadeEntities[0]);
		}
	}
	void OnUpdate(float dt) override{
		auto* comp = GetComponent<CustomScriptComponent>();
		if(GetKeyDown(VK_RETURN)){
			fade->Active = true;
		}
		if(fade->Active && fade->FadeTime <= fade->Timer){
			LoadScene(sceneName);
		}
	}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}
private:
	std::string sceneName = "Asset/Scene/scene.yaml"; // デフォルトのシーン名
};
