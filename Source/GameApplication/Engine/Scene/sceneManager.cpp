// Engine/Scene/sceneManager.cpp
#include "sceneManager.h"
#include "scene.h"

#include "Engine/DebugTools/debugSystem.h"

void SceneManager::Initialize(SceneManagerContext sceneContext){
	m_SceneContext = sceneContext;
}

void SceneManager::Update(float deltaTime){
	if(m_activeScene){
		m_activeScene->Update(deltaTime);
	}
}

void SceneManager::FixedUpdate(float fixedDeltaTime){
	if(m_activeScene){
		m_activeScene->FixedUpdate(fixedDeltaTime);
	}
}

void SceneManager::Render(){
	if(m_activeScene){
		m_activeScene->Render();
	}
}

void SceneManager::Shutdown(){
	if(m_activeScene){
		m_activeScene->Shutdown();
		m_activeScene.reset();
	}
}

void SceneManager::LoadScene(std::shared_ptr<Scene> scene){

	m_SceneContext.debug->LOG_INFO(u8"Sceneを読み込みます...");
	if(m_activeScene){
		m_SceneContext.debug->LOG_DEBUG(u8"ActiveSceneを終了します");
		m_activeScene->Shutdown();
		m_activeScene.reset();

	} else{
		m_SceneContext.debug->LOG_WARNING(u8"ActiveSceneの終了はスキップされました ActiveSceneが存在しないか見つけられませんでした");
	}
	m_activeScene = scene;
	if(m_activeScene){
		m_activeScene->Initialize(&m_SceneContext);
	}
}

std::shared_ptr<Scene> SceneManager::GetActiveScene() const{
	return m_activeScene;
}
