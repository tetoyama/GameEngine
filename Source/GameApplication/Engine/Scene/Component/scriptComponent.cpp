// =======================================================================
//
// scriptComponent.cpp
//
// =======================================================================
#include "scriptComponent.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/systemRegistry.h"
#include "Scene/System/Script/ScriptSystem.h"
#include "DebugTools/DebugSystem.h"
#include "Backends/ImGuiFunc.h"

#include <cstring>
#include <string>
#include <type_traits>
#include <utility>

ScriptComponent::~ScriptComponent(){
	DestroyAllScripts();
}

ScriptComponent::ScriptComponent(ScriptComponent&& other) noexcept
	: scripts(std::move(other.scripts))
	, m_moduleAPI(other.m_moduleAPI){
	other.scripts.clear();
	other.m_moduleAPI = {};
}

ScriptComponent& ScriptComponent::operator=(ScriptComponent&& other) noexcept{
	if(this == &other){
		return *this;
	}

	DestroyAllScripts();

	scripts = std::move(other.scripts);
	m_moduleAPI = other.m_moduleAPI;

	other.scripts.clear();
	other.m_moduleAPI = {};
	return *this;
}

void ScriptComponent::DestroyAllScripts(){
	for(auto& [name, script] : scripts){
		if(!script) continue;

		script->Stop();

		if(m_moduleAPI.destroy){
			m_moduleAPI.destroy(script);
		} else {
			// 異なるCRTで生成された可能性があるため、Engine側deleteは行わない。
			OutputDebugStringA(
				("ScriptComponent: destroy function is missing for script [" +
				 name + "]\n").c_str()
			);
		}
	}

	scripts.clear();
}

YAML::Node ScriptComponent::encode(){
	YAML::Node node;
	if(!m_moduleAPI.serialize || !m_moduleAPI.freeBuffer){
		return node;
	}

	for(auto& [name, script] : scripts){
		if(!script) continue;

		char* stateData = nullptr;
		size_t stateSize = 0;
		if(!m_moduleAPI.serialize(script, &stateData, &stateSize)){
			continue;
		}

		const std::string serialized = stateData
			? std::string(stateData, stateSize)
			: std::string{};
		m_moduleAPI.freeBuffer(stateData);

		try {
			node[name] = serialized.empty()
				? YAML::Node{}
				: YAML::Load(serialized);
		} catch(const std::exception& exception){
			OutputDebugStringA(
				("ScriptComponent: failed to parse serialized state for [" +
				 name + "]: " + exception.what() + "\n").c_str()
			);
		}
	}
	return node;
}

bool ScriptComponent::decode(SceneContext* context, const YAML::Node& node){
	if(!context || !context->system){
		return false;
	}

	auto* scriptSystem = context->system->GetSystem<ScriptSystem>();
	if(!scriptSystem){
		return false;
	}

	DestroyAllScripts();
	SetModuleAPI(scriptSystem->GetModuleAPI());
	if(!m_moduleAPI.IsValid()){
		return false;
	}

	for(auto it : node){
		const std::string name = it.first.as<std::string>();
		IScriptComponent* script = scriptSystem->Create(name.c_str());
		if(!script) continue;

		YAML::Emitter emitter;
		emitter << it.second;
		if(!emitter.good()){
			scriptSystem->Destroy(script);
			continue;
		}

		const char* stateText = emitter.c_str();
		const size_t stateSize = std::strlen(stateText);
		if(!m_moduleAPI.deserialize(script, stateText, stateSize)){
			scriptSystem->Destroy(script);
			continue;
		}

		scripts.emplace(name, script);
	}
	return true;
}

void ScriptComponent::inspector(SceneContext* context){
	ImGui::Text("Script Component");

	static char scriptNameInput[128] = {};
	ImGui::InputText("Add Script", scriptNameInput, sizeof(scriptNameInput));
	ImGui::SameLine();
	if(ImGui::Button("Add")){
		AddScript(scriptNameInput, context);
	}

	for(auto& [name, script] : scripts){
		if(!script) continue;

		if(ImGui::TreeNode(name.c_str())){
			auto& params = script->GetParams();
			for(auto& param : params){
				std::visit([&](auto& value){
					using T = std::decay_t<decltype(value)>;

					if constexpr(std::is_same_v<T, int>){
						int temporary = value;
						if(ImGui::InputInt(param.name.c_str(), &temporary)){
							script->SetParam(param.name, temporary);
						}
					} else if constexpr(std::is_same_v<T, float>){
						float temporary = value;
						if(ImGui::InputFloat(param.name.c_str(), &temporary)){
							script->SetParam(param.name, temporary);
						}
					} else if constexpr(std::is_same_v<T, bool>){
						bool temporary = value;
						if(ImGui::UndoCheckbox(param.name.c_str(), &temporary)){
							script->SetParam(param.name, temporary);
						}
					} else if constexpr(std::is_same_v<T, std::string>){
						char buffer[256] = {};
						std::strncpy(buffer, value.c_str(), sizeof(buffer) - 1);

						if(ImGui::InputText(param.name.c_str(), buffer, sizeof(buffer))){
							script->SetParam(param.name, std::string(buffer));
						}
					}
				}, param.value);
			}

			ImGui::TreePop();
		}
	}
}

bool ScriptComponent::AddScript(const char* scriptName, SceneContext* context){
	if(!context || !context->system || !scriptName || scriptName[0] == '\0'){
		if(context && context->manager && context->manager->debug){
			context->manager->debug->LOG_DEBUG("Script name empty or context invalid");
		}
		return false;
	}

	if(scripts.contains(scriptName)){
		if(context->manager && context->manager->debug){
			context->manager->debug->LOG_DEBUG("Script already exists");
		}
		return false;
	}

	auto* scriptSystem = context->system->GetSystem<ScriptSystem>();
	if(!scriptSystem){
		if(context->manager && context->manager->debug){
			context->manager->debug->LOG_DEBUG("ScriptSystem not found");
		}
		return false;
	}

	IScriptComponent* script = scriptSystem->Create(scriptName);
	if(!script){
		if(context->manager && context->manager->debug){
			context->manager->debug->LOG_DEBUG("Script create failed");
		}
		return false;
	}

	SetModuleAPI(scriptSystem->GetModuleAPI());
	if(!m_moduleAPI.IsValid()){
		scriptSystem->Destroy(script);
		return false;
	}

	scripts.emplace(scriptName, script);
	return true;
}
