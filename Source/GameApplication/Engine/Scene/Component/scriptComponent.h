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

#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

using ScriptDestroyFunction = void(*)(IScriptComponent*);

// Script DLLが生成したIScriptComponentの所有権を保持するコンポーネント。
// 生成と破棄は必ず同じDLL内で完結させる。
class ScriptComponent: public IComponent {
public:
	ScriptComponent() = default;
	~ScriptComponent() override;

	ScriptComponent(const ScriptComponent&) = delete;
	ScriptComponent& operator=(const ScriptComponent&) = delete;

	ScriptComponent(ScriptComponent&& other) noexcept;
	ScriptComponent& operator=(ScriptComponent&& other) noexcept;

	YAML::Node encode() override;
	bool decode(SceneContext* context, const YAML::Node& node) override;
	void inspector(SceneContext* context) override;

	bool AddScript(const char* scriptName, SceneContext* context);

	void SetDestroyFunction(ScriptDestroyFunction destroyFunction) {
		m_destroyFunction = destroyFunction;
	}

	ScriptDestroyFunction GetDestroyFunction() const {
		return m_destroyFunction;
	}

	void DestroyAllScripts();

	std::unordered_map<std::string, IScriptComponent*> scripts;

private:
	ScriptDestroyFunction m_destroyFunction = nullptr;
};
