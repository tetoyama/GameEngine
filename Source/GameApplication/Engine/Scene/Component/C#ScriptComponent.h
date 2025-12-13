#pragma once  
#include "buildSetting.h"

#include "Interface/IComponent.h"  
#include <string>  
#include <memory>  
//#include <vcclr.h> // ← C++/CLI マネージ型を保持するために必要  
//#include "Backends/ScriptWrapper/ScriptWrapper.h" // ← C++/CLI DLL プロジェクトのヘッダー  

#include "Backends/ScriptWrapper/NativeScriptHandle.h" // ← C++/CLI DLL プロジェクトのヘッダー

#ifdef _DEBUG_BUILD
	#pragma comment(lib, "x64/Debug/ScriptWrapper.lib") // Debugビルド用
#else
	#pragma comment(lib, "x64/Release/ScriptWrapper.lib") // Releaseビルド用
#endif

class CSharpScriptComponent: public IComponent {
public:
	CSharpScriptComponent() = default;
	~CSharpScriptComponent() override{
		OnStop();
		if(handle){
			delete handle;
			handle = nullptr;
		}
	}

	YAML::Node encode() override{
		YAML::Node node;
		node["ScriptName"] = scriptName;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(node["ScriptName"])
			scriptName = node["ScriptName"].as<std::string>();
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text("Script Component");
		ImGui::InputText("Script", &scriptName[0], scriptName.capacity() + 1);
	}

	void OnStart(){
		if(!handle)
			handle = new NativeScriptHandle(scriptName);
		handle->OnStart();
		initialzied = true;
	}
	void OnUpdate(float dt){
		if(handle) handle->OnUpdate(dt);
	}
	void OnFixedUpdate(float dt){
		if(handle) handle->OnFixedUpdate(dt);
	}
	void OnDraw(){
		if(handle) handle->OnDraw();
	}
	void OnStop(){
		if(handle) handle->OnStop();
	}

	void SetScriptName(const std::string& name) {
		scriptName = name;
		if(handle) {
			delete handle; // 既存のハンドルを削除
			handle = new NativeScriptHandle(scriptName); // 新しいハンドルを作成
			initialzied = false; // 初期化フラグをリセット
		}
	}

	bool IsInitialized() const {
		return initialzied;
	}

private:
	std::string scriptName = "PlayerScript";
	NativeScriptHandle* handle = nullptr;
	bool initialzied = false;

};
