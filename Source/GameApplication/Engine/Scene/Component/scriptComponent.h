// =======================================================================
//
// scriptComponent.h
//
// =======================================================================
#pragma once

#include "Interface/IComponent.h"
#include "Interface/IScriptComponent.h"
#include <backends/yaml-cpp/yaml.h>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>

using ScriptDestroyFunction = void(*)(IScriptComponent*);
using ScriptSerializeFunction = bool(*)(IScriptComponent*, char**, size_t*);
using ScriptFreeBufferFunction = void(*)(char*);
using ScriptDeserializeFunction = bool(*)(IScriptComponent*, const char*, size_t);

struct ScriptModuleAPI {
	ScriptDestroyFunction destroy = nullptr;
	ScriptSerializeFunction serialize = nullptr;
	ScriptFreeBufferFunction freeBuffer = nullptr;
	ScriptDeserializeFunction deserialize = nullptr;

	bool IsValid() const {
		return destroy && serialize && freeBuffer && deserialize;
	}
};

// Script DLLが生成したIScriptComponentの所有権を保持するコンポーネント。
// 生成・破棄・状態変換は必ず同じDLL内で完結させる。
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

	void SetModuleAPI(const ScriptModuleAPI& moduleAPI) {
		m_moduleAPI = moduleAPI;
	}

	const ScriptModuleAPI& GetModuleAPI() const {
		return m_moduleAPI;
	}

	void DestroyAllScripts();

	std::unordered_map<std::string, IScriptComponent*> scripts;

private:
	ScriptModuleAPI m_moduleAPI{};
};
