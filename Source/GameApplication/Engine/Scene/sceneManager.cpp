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
	if(m_activeScene){
		m_activeScene->Update(deltaTime);
	}
	if(m_NeedSceneChange){
		LoadScene(m_NextScene);
		m_NeedSceneChange = false;
		m_NextScene.reset();
	}
}

void SceneManager::FixedUpdate(float fixedDeltaTime){
	if(m_activeScene){
		m_activeScene->FixedUpdate(fixedDeltaTime);
	}
}

void SceneManager::Draw(){
	if(m_activeScene){
		m_activeScene->Draw();
	}
}

void SceneManager::Shutdown(){
	if(m_activeScene){
		m_activeScene->Shutdown();
		m_activeScene.reset();
	}
}

void SceneManager::LoadScene(std::shared_ptr<Scene> scene){

	m_SceneContext.debug->LOG_INFO("Sceneを読み込みます...");
	if(m_activeScene){
		m_SceneContext.debug->LOG_DEBUG("ActiveSceneを終了します");
		m_activeScene->Shutdown();
		m_activeScene.reset();

	} else{
		m_SceneContext.debug->LOG_WARNING("ActiveSceneの終了はスキップされました ActiveSceneが存在しないか見つけられませんでした");
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

void SceneManager::SaveScene(){
	if (m_activeScene) {
		m_activeScene->Save();
		m_SceneContext.debug->LOG_INFO("Sceneを保存しました");
	} else {
		m_SceneContext.debug->LOG_WARNING("ActiveSceneが存在しないため、保存は行われませんでした");
	}
}

void SceneManager::LoadFromYAMLFile(){
	auto scene = std::make_shared<Scene>();

	scene->Initialize(&m_SceneContext);
	if(scene->LoadFromYAMLFile()){
		if(m_activeScene){
			m_SceneContext.debug->LOG_DEBUG("ActiveSceneを終了します");
			m_activeScene->Shutdown();
			m_activeScene.reset();
		}
		m_activeScene = scene;
		m_SceneContext.debug->LOG_DEBUG("OpenScene");

		m_SceneContext.resource->ClearAllUnused();
	} else{
		scene->Shutdown();
		scene.reset();
	}
	OpenFlag = true;
}
