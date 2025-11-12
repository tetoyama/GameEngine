// Engine/Scene/sceneManager.cpp
#include "sceneManager.h"
#include "scene.h"

#include "Engine/DebugTools/debugSystem.h"
#include "Engine/Resources/resourceService.h"

void SceneManager::Initialize(ManagerContext sceneContext){
	m_SceneContext = sceneContext;
}

void SceneManager::Update(float deltaTime){
	if (OpenFlag) {
		OpenFlag = false;
	}
	for(auto& [name, scene] : m_activeScenes){
		scene->Update(deltaTime);
	}
	if(m_NeedSceneChange){
		LoadScene(m_NextScene);
		m_NextScene->SetState(SceneState::Playing);
		m_NeedSceneChange = false;
		m_NextScene.reset();
	}
}

void SceneManager::FixedUpdate(float fixedDeltaTime){
	for(auto& [name, scene] : m_activeScenes){
		scene->FixedUpdate(fixedDeltaTime);
	}
}

void SceneManager::Draw(){
	for(auto& [name, scene] : m_activeScenes){
		scene->Draw();
	}
}

void SceneManager::Shutdown(){
	for(auto& [name, scene] : m_activeScenes){
		scene->Shutdown();
		scene.reset();
	}
	m_activeScenes.clear();
}

void SceneManager::AddScene(std::shared_ptr<Scene> scene){
	m_activeScenes[scene->SceneName] = scene;
}

void SceneManager::LoadScene(std::shared_ptr<Scene> scene){

	m_SceneContext.debug->LOG_INFO("Sceneを読み込みます...");
	if(m_activeScene){
		m_SceneContext.debug->LOG_DEBUG("ActiveSceneを終了します");
		m_activeScene->Shutdown();
		m_activeScene.reset();

	} else{
		//m_SceneContext.debug->LOG_WARNING("ActiveSceneの終了はスキップされました ActiveSceneが存在しないか見つけられませんでした");
	}
	m_activeScene = scene;
	if(m_activeScene){
		m_activeScene->Initialize(&m_SceneContext);
	}
	m_SceneContext.resource->ClearAllUnused();
}

void SceneManager::DeferredLoadScene(std::shared_ptr<Scene> scene){
	m_NextScene.reset();
	m_NeedSceneChange = true;
	m_NextScene = scene;
}

void SceneManager::SaveScenes(){
	for(auto& scene : m_activeScenes){
		scene->Save();
	}
}

void SceneManager::LoadFromYAMLFile(){
	auto scene = std::make_shared<Scene>();
	if(m_activeScene){
		m_SceneContext.debug->LOG_DEBUG("ActiveSceneを終了します");
		m_activeScene->Shutdown();
	}
	scene->Initialize(&m_SceneContext);
	if(scene->LoadFromYAMLFile()){
		m_activeScene.reset();

		m_activeScene = scene;
		m_SceneContext.debug->LOG_DEBUG("OpenScene");

		m_SceneContext.resource->ClearAllUnused();
	} else{
		scene->Shutdown();
		scene.reset();
		m_activeScene->Initialize(&m_SceneContext);
	}
	OpenFlag = true;
}

bool SceneManager::LoadFromYAML(const std::string& filePath){
	auto scene = std::make_shared<Scene>();

	m_SceneContext.debug->LOG_INFO("Sceneを読み込みます...");
	if(m_activeScene){
		m_SceneContext.debug->LOG_DEBUG("ActiveSceneを終了します");
		m_activeScene->Shutdown();
		m_activeScene.reset();

	} else{
		//m_SceneContext.debug->LOG_WARNING("ActiveSceneの終了はスキップされました ActiveSceneが存在しないか見つけられませんでした");
	}
	m_activeScene = scene;
	if(m_activeScene){
		scene->Initialize(&m_SceneContext);
		scene->ResetAll();
		scene->LoadSceneFromYAML(filePath);
		scene->SetState(SceneState::Playing);
	}
	m_SceneContext.resource->ClearAllUnused();
	OpenFlag = true;

	return true;
}
