#include "ScriptSystem.h"
#include "Scene/Component/ScriptComponent.h"
#include "Component/componentList.h"

void ScriptSystem::Start(){
	ForEachScript([](IScriptComponent* script){
		script->Start();
				  });
}

void ScriptSystem::Stop(){
	ForEachScript([](IScriptComponent* script){
		script->Stop();
				  });
}

void ScriptSystem::Update(float deltaTime){
	ForEachScript([deltaTime](IScriptComponent* script){
		script->Update(deltaTime);
				  });
}

void ScriptSystem::FixedUpdate(float fixedDeltaTime){
	ForEachScript([fixedDeltaTime](IScriptComponent* script){
		script->FixedUpdate(fixedDeltaTime);
				  });
}

void ScriptSystem::EditorUpdate(float deltaTime){
	ForEachScript([deltaTime](IScriptComponent* script){
		script->EditorUpdate(deltaTime);
				  });
}

void ScriptSystem::Draw(){

	if (m_setImGuiContextFunc) {
		// ImGui コンテキストをスクリプト側にセット
		m_setImGuiContextFunc((void*)ImGui::GetCurrentContext());
	}

	ForEachScript([](IScriptComponent* script){
		script->Draw();
				  });
}

bool ScriptSystem::ReloadScriptDLL(const char* dllPath){
	// =========================
	// 1. 既存 DLL を解放
	// =========================
	if(m_scriptModule){
		FreeLibrary(m_scriptModule);
		m_scriptModule = nullptr;
		m_scriptBridge = nullptr;
	}

	// =========================
	// 2. DLL をロード
	// =========================
	m_scriptModule = LoadLibraryA(dllPath);
	if(!m_scriptModule){
		return false;
	}

	// =========================
	// 3. CreateScript 関数取得
	// =========================
	auto createFunc =
		reinterpret_cast<CreateScriptFunc>(
			GetProcAddress(m_scriptModule, "CreateScript"));

	if(!createFunc){
		FreeLibrary(m_scriptModule);
		m_scriptModule = nullptr;
		return false;
	}

	// =========================
	// 4. ScriptBridge を差し替え
	// =========================
	m_scriptBridge =
		[createFunc](const char* scriptName){
		IScriptComponent* raw = createFunc(scriptName);
		return raw
			? (IScriptComponent*)(raw)
			: nullptr;
		};

	auto rawFunc = reinterpret_cast<SetImGuiContextFunc>(
		GetProcAddress(m_scriptModule, "SetImGuiContext"));

	m_setImGuiContextFunc = [rawFunc](void* ctx) {
		if (rawFunc) rawFunc(ctx);
	};

	return true;
}


void ScriptSystem::Initialize(){

	// コンポーネントの登録
#define REGISTER(T, STORAGE) \
    RegisterEngineComponent<T>(#T);
	COMPONENT_LIST(REGISTER)
#undef REGISTER

	ReloadScriptDLL("Script.dll");
}

void ScriptSystem::Finalize(){

}

template<typename Func>
void ScriptSystem::ForEachScript(Func&& func){
	for(auto& [_, scene] : m_context->sceneManager->GetActiveScenes()){
		auto* ctx = scene->GetSceneContext();
		auto entities =
			ctx->component->FindEntitiesWithComponent<ScriptComponent>();

		for(Entity e : entities){
			auto* sc = ctx->component->GetComponent<ScriptComponent>(e);
			if(!sc) continue;

			for(auto& [_, script] : sc->scripts){
				if(!script) continue;

				script->entity = e;
				script->context = ctx;

				func(script);
			}
		}
	}
}