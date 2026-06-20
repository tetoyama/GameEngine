// =======================================================================
//
// ScriptSystem.cpp
//
// =======================================================================
#include "ScriptSystem.h"
#include "Scene/Component/scriptComponent.h"
#include "Component/componentList.h"
#include "Scene/Reference/EntityRef.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
	struct ScriptReloadRecord {
		SceneContext* context = nullptr;
		Entity entity{};
		std::string serializedState;
	};

	void LogScriptSystemError(const std::string& message){
		OutputDebugStringA(("ScriptSystem: " + message + "\n").c_str());
	}
}

void ScriptSystem::Start(){
	m_commandCommitSystem.PrepareBuffers();

	ForEachScript(SystemTaskDomain::Frame, [](IScriptComponent* script){
		script->Start();
	});

	m_commandCommitSystem.CommitNow();
}

void ScriptSystem::Stop(){
	ForEachScript(SystemTaskDomain::Frame, [](IScriptComponent* script){
		script->Stop();
	});

	m_commandCommitSystem.CommitNow();
}

void ScriptSystem::Update(float deltaTime){
	ForEachScript(SystemTaskDomain::Frame, [deltaTime](IScriptComponent* script){
		script->Update(deltaTime);
	});
}

void ScriptSystem::FixedUpdate(float fixedDeltaTime){
	ForEachScript(SystemTaskDomain::Fixed, [fixedDeltaTime](IScriptComponent* script){
		script->FixedUpdate(fixedDeltaTime);
	});
}

void ScriptSystem::EditorUpdate(float deltaTime){
	ForEachScript(SystemTaskDomain::Editor, [deltaTime](IScriptComponent* script){
		script->EditorUpdate(deltaTime);
	});
}

void ScriptSystem::Draw(){
	if(m_setImGuiContextFunc){
		m_setImGuiContextFunc(static_cast<void*>(ImGui::GetCurrentContext()));
	}

	ForEachScript(SystemTaskDomain::Render, [](IScriptComponent* script){
		script->Draw();
	});
}

bool ScriptSystem::ReloadScriptDLL(const char* originalPath){
	if(!originalPath || originalPath[0] == '\0'){
		LogScriptSystemError("DLL path is empty.");
		return false;
	}

	std::error_code error;
	const std::filesystem::path sourcePath =
		std::filesystem::absolute(originalPath, error);
	if(error || !std::filesystem::exists(sourcePath)){
		LogScriptSystemError("Script DLL does not exist: " + std::string(originalPath));
		return false;
	}

	// 元DLLを直接ロードするとビルド時に上書きできないため、一意な一時名へコピーする。
	const std::filesystem::path reloadDirectory =
		sourcePath.parent_path() / ".hotreload";
	std::filesystem::create_directories(reloadDirectory, error);
	if(error){
		LogScriptSystemError("Failed to create hot reload directory.");
		return false;
	}

	const std::wstring copiedFileName =
		sourcePath.stem().wstring() +
		L"_" + std::to_wstring(GetCurrentProcessId()) +
		L"_" + std::to_wstring(++m_reloadGeneration) +
		sourcePath.extension().wstring();
	const std::filesystem::path copiedPath = reloadDirectory / copiedFileName;

	std::filesystem::copy_file(
		sourcePath,
		copiedPath,
		std::filesystem::copy_options::overwrite_existing,
		error
	);
	if(error){
		LogScriptSystemError("Failed to copy Script DLL for hot reload.");
		return false;
	}

	HMODULE newModule = LoadLibraryW(copiedPath.c_str());
	if(!newModule){
		std::filesystem::remove(copiedPath, error);
		LogScriptSystemError("LoadLibrary failed for copied Script DLL.");
		return false;
	}

	CreateScriptFunc newCreateFunction = reinterpret_cast<CreateScriptFunc>(
		GetProcAddress(newModule, "CreateScript")
	);

	ScriptModuleAPI newModuleAPI;
	newModuleAPI.destroy = reinterpret_cast<ScriptDestroyFunction>(
		GetProcAddress(newModule, "DestroyScript")
	);
	newModuleAPI.serialize = reinterpret_cast<ScriptSerializeFunction>(
		GetProcAddress(newModule, "SerializeScriptState")
	);
	newModuleAPI.freeBuffer = reinterpret_cast<ScriptFreeBufferFunction>(
		GetProcAddress(newModule, "FreeScriptStateBuffer")
	);
	newModuleAPI.deserialize = reinterpret_cast<ScriptDeserializeFunction>(
		GetProcAddress(newModule, "DeserializeScriptState")
	);

	auto newImGuiContextFunction = reinterpret_cast<SetImGuiContextFunc>(
		GetProcAddress(newModule, "SetImGuiContext")
	);

	// 新DLLを完全に検証するまで旧DLLと旧Scriptには触れない。
	if(!newCreateFunction || !newModuleAPI.IsValid()){
		FreeLibrary(newModule);
		std::filesystem::remove(copiedPath, error);
		LogScriptSystemError("Required Script DLL exports are missing.");
		return false;
	}

	std::vector<ScriptReloadRecord> records;
	if(m_scriptModule && m_context && m_context->sceneManager){
		for(auto& [sceneName, scene] : m_context->sceneManager->GetActiveScenes()){
			if(!scene) continue;

			SceneContext* context = scene->GetSceneContext();
			if(!context || !context->component || !context->entity) continue;

			const auto entities =
				context->component->FindEntitiesWithComponent<ScriptComponent>();
			for(Entity entity : entities){
				if(!context->entity->IsAlive(entity)) continue;

				ScriptComponent* component =
					context->component->GetComponent<ScriptComponent>(entity);
				if(!component) continue;

				component->SetModuleAPI(m_moduleAPI);

				YAML::Emitter emitter;
				emitter << component->encode();
				if(!emitter.good()){
					LogScriptSystemError("Failed to serialize ScriptComponent before reload.");
					continue;
				}

				records.push_back({context, entity, emitter.c_str()});
			}
		}
	}

	m_commandCommitSystem.PrepareBuffers();

	// 状態退避後、旧DLLが有効な間に旧Scriptを破棄する。
	DestroyAllScriptInstances();

	HMODULE oldModule = m_scriptModule;
	const std::wstring oldModulePath = m_loadedModulePath;

	m_scriptModule = newModule;
	m_createScriptFunc = newCreateFunction;
	m_moduleAPI = newModuleAPI;
	m_loadedModulePath = copiedPath.wstring();

	if(newImGuiContextFunction){
		m_setImGuiContextFunc = [newImGuiContextFunction](void* context){
			newImGuiContextFunction(context);
		};
	} else {
		m_setImGuiContextFunc = nullptr;
	}

	if(oldModule){
		FreeLibrary(oldModule);
	}
	if(!oldModulePath.empty()){
		std::filesystem::remove(oldModulePath, error);
	}

	const bool shouldStartScripts =
		m_context && m_context->sceneManager &&
		m_context->sceneManager->State != SceneManagerState::Stopped;

	for(const ScriptReloadRecord& record : records){
		if(!record.context || !record.context->component ||
			!record.context->entity ||
			!record.context->entity->IsAlive(record.entity)){
			continue;
		}

		ScriptComponent* component =
			record.context->component->GetComponent<ScriptComponent>(record.entity);
		if(!component) continue;

		component->SetModuleAPI(m_moduleAPI);

		try {
			const YAML::Node state = record.serializedState.empty()
				? YAML::Node{}
				: YAML::Load(record.serializedState);
			if(!component->decode(record.context, state)){
				LogScriptSystemError("Failed to restore ScriptComponent after reload.");
				continue;
			}
		} catch(const std::exception& exception){
			LogScriptSystemError(
				"Failed to parse ScriptComponent state after reload: " +
				std::string(exception.what())
			);
			continue;
		}

		if(shouldStartScripts){
			for(auto& [scriptName, script] : component->scripts){
				if(!script) continue;
				script->ref = EntityRef(record.entity, record.context);
				script->Start();
			}
		}
	}

	m_commandCommitSystem.CommitNow();
	return true;
}

void ScriptSystem::Initialize(){
#define REGISTER(T, STORAGE) \
    RegisterEngineComponent<T>(#T);
	COMPONENT_LIST(REGISTER)
#undef REGISTER

	ReloadScriptDLL("Script/Script.dll");
}

void ScriptSystem::Finalize(){
	m_commandCommitSystem.CommitNow();
	DestroyAllScriptInstances();
	UnloadCurrentModule();
}

void ScriptSystem::DestroyAllScriptInstances(){
	if(!m_context || !m_context->sceneManager){
		return;
	}

	for(auto& [sceneName, scene] : m_context->sceneManager->GetActiveScenes()){
		if(!scene) continue;

		SceneContext* context = scene->GetSceneContext();
		if(!context || !context->component) continue;

		const auto entities =
			context->component->FindEntitiesWithComponent<ScriptComponent>();

		for(Entity entity : entities){
			ScriptComponent* component =
				context->component->GetComponent<ScriptComponent>(entity);
			if(!component) continue;

			component->SetModuleAPI(m_moduleAPI);
			component->DestroyAllScripts();
			component->SetModuleAPI({});
		}
	}
}

void ScriptSystem::UnloadCurrentModule(){
	m_createScriptFunc = nullptr;
	m_moduleAPI = {};
	m_setImGuiContextFunc = nullptr;

	if(m_scriptModule){
		FreeLibrary(m_scriptModule);
		m_scriptModule = nullptr;
	}

	if(!m_loadedModulePath.empty()){
		std::error_code error;
		std::filesystem::remove(m_loadedModulePath, error);
		m_loadedModulePath.clear();
	}
}

template<typename Func>
void ScriptSystem::ForEachScript(SystemTaskDomain domain, Func&& func){
	struct Entry {
		IScriptComponent* script = nullptr;
		Entity entity{};
		std::string name;
	};

	std::vector<Entry> entries;

	for(auto& [sceneName, scene] : m_context->sceneManager->GetActiveScenes()){
		auto* context = scene->GetSceneContext();
		auto entities =
			context->component->FindEntitiesWithComponent<ScriptComponent>();

		for(Entity entity : entities){
			auto* component =
				context->component->GetComponent<ScriptComponent>(entity);
			if(!component) continue;

			if(!component->GetModuleAPI().IsValid()){
				component->SetModuleAPI(m_moduleAPI);
			}

			for(auto& [scriptName, script] : component->scripts){
				if(!script) continue;

				if(!script->HasRegistrationOrder()){
					script->SetRegistrationOrder(m_nextScriptRegistrationOrder++);
				}

				script->ref = EntityRef(entity, context);
				entries.push_back({script, entity, scriptName});
			}
		}
	}

	std::sort(
		entries.begin(),
		entries.end(),
		[domain](const Entry& lhs, const Entry& rhs){
			const SystemTaskOrder lhsOrder =
				lhs.script->GetExecutionOrder(domain);
			const SystemTaskOrder rhsOrder =
				rhs.script->GetExecutionOrder(domain);

			if(IsScriptOrderEarlier(lhsOrder, rhsOrder)) return true;
			if(IsScriptOrderEarlier(rhsOrder, lhsOrder)) return false;

			if(lhs.entity.GetPackedValue() != rhs.entity.GetPackedValue()){
				return lhs.entity.GetPackedValue() < rhs.entity.GetPackedValue();
			}
			return lhs.name < rhs.name;
		}
	);

	for(const Entry& entry : entries){
		func(entry.script);
	}
}
