// =======================================================================
//
// CustomScriptSystem.cpp
//
// =======================================================================
#include "CustomScriptSystem.h"
#include "Component/CustomScriptComponent.h"

#include "Scene.h"
#include "sceneManager.h"
#include "DebugTools/debugSystem.h"

#include "Registry/entityRegistry.h"
#include "Registry/componentRegistry.h"

#include <algorithm>
#include <string>
#include <vector>

void CustomScriptSystem::ForEachScriptOrdered(
	SystemTaskDomain domain,
	const std::function<void(CustomScriptComponent*)>& callback
){
	struct Entry {
		Entity entity{};
		SceneContext* context = nullptr;
		CustomScriptComponent* script = nullptr;
		std::string name;
	};

	std::vector<Entry> entries;

	for(auto& [sceneName, scene] : m_context->sceneManager->GetActiveScenes()){
		auto* context = scene->GetSceneContext();
		auto scripts =
			context->component->GetAllBaseComponents<CustomScriptComponent>();

		for(auto& [entity, script] : scripts){
			if(script){
				entries.push_back({
					entity,
					context,
					script,
					script->GetScriptName()
				});
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
		entry.script->SetContext(entry.context, entry.entity);
		callback(entry.script);
	}
}

void CustomScriptSystem::Initialize(){
	ForEachScriptOrdered(
		SystemTaskDomain::Frame,
		[](CustomScriptComponent* script){
			if(!script->IsInitialized()){
				script->Initialize();
			}
		}
	);
}

void CustomScriptSystem::Finalize(){
	ForEachScriptOrdered(
		SystemTaskDomain::Frame,
		[](CustomScriptComponent* script){
			if(!script->IsInitialized()){
				script->Initialize();
			}
			script->Stop();
		}
	);
}

void CustomScriptSystem::Start(){
	ForEachScriptOrdered(
		SystemTaskDomain::Frame,
		[](CustomScriptComponent* script){
			if(!script->IsInitialized()){
				script->Initialize();
			}
			script->Start();
		}
	);
}

void CustomScriptSystem::Update(float deltaTime){
	ForEachScriptOrdered(
		SystemTaskDomain::Frame,
		[deltaTime](CustomScriptComponent* script){
			if(!script->IsInitialized()){
				script->Initialize();
				script->Start();
			}
			script->Update(deltaTime);
		}
	);
}

void CustomScriptSystem::FixedUpdate(float fixedDeltaTime){
	ForEachScriptOrdered(
		SystemTaskDomain::Fixed,
		[fixedDeltaTime](CustomScriptComponent* script){
			if(!script->IsInitialized()){
				script->Initialize();
			}
			script->FixedUpdate(fixedDeltaTime);
		}
	);
}

void CustomScriptSystem::Draw(){
	ForEachScriptOrdered(
		SystemTaskDomain::Render,
		[](CustomScriptComponent* script){
			if(!script->IsInitialized()){
				script->Initialize();
			}
			script->Draw();
		}
	);
}

void CustomScriptSystem::EditorUpdate(float deltaTime){
	ForEachScriptOrdered(
		SystemTaskDomain::Editor,
		[deltaTime](CustomScriptComponent* script){
			if(!script->IsInitialized()){
				script->Initialize();
			}
			script->EditorUpdate(deltaTime);
		}
	);
}
