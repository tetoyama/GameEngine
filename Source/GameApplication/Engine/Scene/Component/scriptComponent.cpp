#include "scriptComponent.h"
#include "Scene/sceneManager.h"
#include "DebugTools/DebugSystem.h"

bool ScriptComponent::decode(SceneContext* context, const YAML::Node& node){
	for(auto it : node){
		const std::string name = it.first.as<std::string>();
		auto script = context->manager->systemRegistry->GetSystem<ScriptSystem>()->Create(name.c_str());
		if(!script) continue;

		script->Decode(it.second);
		scripts[name] = std::move(script);
	}
	return true;
}

void ScriptComponent::inspector(SceneContext* context){
	ImGui::Text("Script Component");

	static char scriptName[128] = {};
	ImGui::Text("Add Script:");
	ImGui::InputText("##script_name_input", scriptName, sizeof(scriptName));
	ImGui::SameLine();

	if(ImGui::Button("Add")){
		AddScript(scriptName, context);
	}

	for(auto& [name, script] : scripts){
		if(ImGui::TreeNode(name.c_str())){
			script->Inspector();
			ImGui::TreePop();
		}
	}
}
bool ScriptComponent::AddScript(const char* scriptName, SceneContext* context){
	if(!scriptName || scriptName[0] == '\0'){
		context->manager->debug->LOG_DEBUG("Script name empty");
		return false;
	}

	if(scripts.find(scriptName) != scripts.end()){
		context->manager->debug->LOG_DEBUG("Script already exists");
		return false;
	}

	auto* scriptSystem = context->system->GetSystem<ScriptSystem>();
	if(!scriptSystem){
		context->manager->debug->LOG_DEBUG("ScriptSystem not found");
		return false;
	}

	IScriptComponent* raw = scriptSystem->Create(scriptName);
	if(!raw){
		context->manager->debug->LOG_DEBUG("Script create failed");
		return false;
	}

	IScriptComponent* script(raw);
	script->context = context;

	scripts.emplace(scriptName, std::move(script));
	return true;
}
