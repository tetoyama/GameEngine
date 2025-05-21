// Engine/Scene/sceneManager.cpp
#include "sceneManager.h"
#include "scene.h"

SceneManager::SceneManager()
	: m_activeScene(nullptr){}

SceneManager::~SceneManager(){
	Shutdown();
}

void SceneManager::Initialize(MainRenderer* mainRenderer){
	// 必要なら初期シーン生成・セット
	m_mainRenderer = mainRenderer;
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
	if(m_activeScene){
		m_activeScene->Shutdown();
	}
	m_activeScene = scene;
	if(m_activeScene){
		m_activeScene->Initialize(m_mainRenderer);
	}
}

std::shared_ptr<Scene> SceneManager::GetActiveScene() const{
	return m_activeScene;
}
