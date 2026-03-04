// =======================================================================
// 
// ScriptSystem.h
// 
// =======================================================================
#pragma once
#include <unordered_map>
#include <functional>
#include <memory>
#include <typeindex>

#include "Scene/Interface/ISystem.h"
#include "Scene/sceneManager.h"
#include "Interface/IScriptComponent.h"
#include "Scene/Component/ScriptComponent.h"
#include "Scene/Registry/componentRegistry.h"

using SetImGuiContextFunc = void(*)(void*);

// C#スクリプトDLLとの連携を管理するシステム
class ScriptSystem: public ISystem
{
public:
	const char* GetSystemName() const override{
		return "ScriptSystem";
	}
	ScriptSystem(SceneManagerContext* context)
		: m_context(context){}

	// =========================
	// ISystem
	// =========================
	void Initialize() override;
	void Finalize() override;

	void Start() override;
	void Stop() override;
	void Update(float deltaTime) override;
	void FixedUpdate(float fixedDeltaTime) override;
	void EditorUpdate(float deltaTime) override;
	void Draw() override;

	// =========================
	// Script 登録
	// =========================
	template<typename T>
	void RegisterEngineComponent(const char* name){
		static_assert(std::is_base_of_v<IComponent, T>,
					  "T must derive from IComponent");
		m_nameToEngineTypeID[name] = ComponentType::Get<T>();
	}

	// =========================
	// Script 生成
	// =========================
	IScriptComponent* Create(const char* scriptName){
		// ここで「スクリプト側プロジェクト」に名前を投げる
		return m_scriptBridge
			? m_scriptBridge(scriptName)
			: nullptr;
	}

	// =========================
	// Engine Component TypeID 取得
	// =========================
	template<typename T>
	ComponentTypeID GetEngineComponentTypeID() const{
		auto it = m_engineTypeIDs.find(std::type_index(typeid(T)));
		if(it == m_engineTypeIDs.end())
			return static_cast<ComponentTypeID>(-1);

		return it->second;
	}

	// DLL リロード
	bool ReloadScriptDLL(const char* dllPath);

	// DLL から渡される関数をセット
	void SetScriptBridge(
		std::function<IScriptComponent*(const char*)> bridge){
		m_scriptBridge = std::move(bridge);
	}

	// ComponentTypeID を名前で引く（Script 用）
	ComponentTypeID GetEngineComponentTypeIDByName(
		const std::string& name) const{
		auto it = m_nameToEngineTypeID.find(name);
		return it != m_nameToEngineTypeID.end()
			? it->second
			: static_cast<ComponentTypeID>(-1);
	}

	void SystemSetting() override {
		if (ImGui::Button("Reload Script DLL")) {
			ReloadScriptDLL("Script/Script.dll");
		}
	}

	bool HasSystemSetting() const override { return true; }

private:
	SceneManagerContext* m_context = nullptr;

	template<typename Func>
	void ForEachScript(Func&& func);

	// 名前 → Engine ComponentTypeID
	std::unordered_map<std::string, ComponentTypeID> m_nameToEngineTypeID;

	// Script型 → Engine ComponentTypeID
	std::unordered_map<std::type_index, ComponentTypeID> m_engineTypeIDs;

	std::function<IScriptComponent*(const char*)> m_scriptBridge = nullptr;

	std::function<void(void*)> m_setImGuiContextFunc = nullptr;

	HMODULE m_scriptModule = nullptr;

	using CreateScriptFunc = IScriptComponent * (*)(const char*);
};
