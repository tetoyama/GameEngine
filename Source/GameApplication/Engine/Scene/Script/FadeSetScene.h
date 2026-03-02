#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

class FadeSetScene: public CustomScriptComponent {
public:

	FadeSetScene(): CustomScriptComponent("FadeSetScene"){}
	ComponentRef<FadeOutSprite> fade;
	YAML::Node encode() override{
		YAML::Node node;
		node["ScriptName"] = scriptName;
		node["SceneName"] = sceneName;
		return node;
	}
	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(node["ScriptName"])
			scriptName = node["ScriptName"].as<std::string>();
		if(node["SceneName"])
			sceneName = node["SceneName"].as<std::string>();
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text(scriptName.c_str());
		ImGui::UndoInputText("Scene Name", &sceneName);
		if(ImGui::BeginDragDropTarget()){
			if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
				const char* droppedPath = (const char*)payload->Data;
				std::string _Path = std::string(droppedPath);
				if(HasExtension(_Path, ".yaml")){
					ImGui::RecordStringChange(&sceneName, sceneName, _Path, "Scene Name");
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	void OnStart() override{
		auto fadeEntities = m_ref.GetScene()->component->FindEntitiesWithComponent<FadeOutSprite>();
		if(!fadeEntities.empty()){
			fade = GetComponentRefFor<FadeOutSprite>(fadeEntities[0]);
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
