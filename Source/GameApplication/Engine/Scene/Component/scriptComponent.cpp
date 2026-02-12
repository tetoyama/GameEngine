#include "scriptComponent.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "DebugTools/DebugSystem.h"
#include "Interface/IScriptComponent.h"

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

void ScriptComponent::inspector(SceneContext* context) {
	ImGui::Text("Script Component");

	static char scriptNameInput[128] = {};
	ImGui::InputText("Add Script", scriptNameInput, sizeof(scriptNameInput));
	ImGui::SameLine();
	if (ImGui::Button("Add")) {
		AddScript(scriptNameInput, context);
	}

	for (auto& [name, script] : scripts) {
		if (ImGui::TreeNode(name.c_str())) {
			// Script 側のパラメータを ImGui で描画

			auto& params = script->GetParams();
			for (auto& p : params) {
				// コピーで取る
				std::visit([&](auto& val) {
					using T = std::decay_t<decltype(val)>;
					if constexpr (std::is_same_v<T, int>) {
						int tmp = val;
						if (ImGui::InputInt(p.name.c_str(), &tmp)) {
							script->SetParam(p.name, tmp);
						}
					} else if constexpr (std::is_same_v<T, float>) {
						float tmp = val;
						if (ImGui::InputFloat(p.name.c_str(), &tmp)) {
							script->SetParam(p.name, tmp);
						}
					} else if constexpr (std::is_same_v<T, bool>) {
						bool tmp = val;
						if (ImGui::Checkbox(p.name.c_str(), &tmp)) {
							script->SetParam(p.name, tmp);
						}
					}
				}, p.value);
			}




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
	//script->context = context;

	scripts.emplace(scriptName, std::move(script));
	return true;
}
