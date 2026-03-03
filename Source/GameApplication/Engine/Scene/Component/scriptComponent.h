// =======================================================================
// 
// scriptComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include "Interface/IScriptComponent.h"
#include <backends/yaml-cpp/yaml.h>

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Registry/systemRegistry.h"
#include "System/Script/ScriptSystem.h"
#include <variant>

class ScriptComponent: public IComponent {
public:
	ScriptComponent() = default;

	~ScriptComponent() override{
		for(auto& [_, script] : scripts){
			script->Stop();
			delete script;
		}
		scripts.clear();
	}

	// コピー禁止
	ScriptComponent(const ScriptComponent&) = delete;
	ScriptComponent& operator=(const ScriptComponent&) = delete;

	// ムーブは許可
	ScriptComponent(ScriptComponent&&) = default;
	ScriptComponent& operator=(ScriptComponent&&) = default;

	YAML::Node encode() override{
		YAML::Node node;
		for(auto& [name, script] : scripts){
			node[name] = script->Encode();
		}
		return node;
	}


	bool decode(SceneContext* context, const YAML::Node& node) override;


	void inspector(SceneContext* context) override;
	bool AddScript(const char* scriptName, SceneContext* context);

	std::unordered_map<
		std::string,
		IScriptComponent*
	> scripts;
};
