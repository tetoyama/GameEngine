// Scene/sceneManager.cpp
#include <memory>
#include <algorithm>

#include "buildSetting.h"
#include "gameApplication.h"

#include "sceneManager.h"
#include "scene.h"

#include "DebugTools/debugSystem.h"
#include "Resources/resourceService.h"

#include "Registry/systemRegistry.h"

#include "System/Transform/transformSystem.h"

#include "System/Render/RenderSystem/renderSystem.h"
#include "System/Render/Camera/cameraSystem.h"
#include "System/Render/Effect/effectSystem.h"
#include "System/Render/Terrain/waveSystem.h"
#include "System/Render/Terrain/terrainSystem.h"
#include "System/Render/Particle/particleSystem.h"

#include "System/Script/C#ScriptSystem.h"
#include "System/Script/CustomScriptSystem.h"

#include "System/Audio/audioSystem.h"

#include "System/Physic/physicSystem.h"
#include "Editor/editorService.h"
#include <Editor/UI/Inspector.h>
#include <Editor/UI/Hierarchy.h>

void SceneManager::Initialize(SceneManagerContext sceneContext){
	
	#ifdef _DEBUG_BUILD

#ifndef _EDITOR
	State = SceneManagerState::Playing;
	OldState = SceneManagerState::Stopped;
#endif // !_EDITOR

	#else
	State = SceneManagerState::Playing;
	OldState = SceneManagerState::Stopped;
	#endif // DEBUG



	m_SceneContext = sceneContext;

	systemRegistry = std::make_shared<SystemRegistry>();

	systemRegistry->RegisterSystem(std::make_unique<TransformSystem>(&m_SceneContext));
	systemRegistry->RegisterSystem(std::make_unique<CameraSystem>(&m_SceneContext));
	systemRegistry->RegisterSystem(std::make_unique<RenderSystem>(&m_SceneContext));
	systemRegistry->RegisterSystem(std::make_unique<AudioSystem>(&m_SceneContext));
	systemRegistry->RegisterSystem(std::make_unique<ParticleSystem>(&m_SceneContext));
	systemRegistry->RegisterSystem(std::make_unique<EffectSystem>(&m_SceneContext));
	systemRegistry->RegisterSystem(std::make_unique<TerrainSystem>(&m_SceneContext));
	systemRegistry->RegisterSystem(std::make_unique<PhysicSystem>(&m_SceneContext));
	systemRegistry->RegisterSystem(std::make_unique<CSharpScriptSystem>(&m_SceneContext));
	systemRegistry->RegisterSystem(std::make_unique<CustomScriptSystem>(&m_SceneContext));
	systemRegistry->RegisterSystem(std::make_unique<WaveSystem>(&m_SceneContext));

	// システムの初期化
	systemRegistry->InitializeAll();
	m_SceneContext.systemRegistry = systemRegistry.get();

	for (auto& [name, scene] : m_activeScenes) {
		scene->Initialize(&m_SceneContext);
	}
}

void SceneManager::Update(float deltaTime){

	if (OldState != State) {
		if (State == SceneManagerState::Paused) {

			m_SceneContext.debug->LOG_INFO("シーンを一時停止します");
			OldState = State;

		} else if (State == SceneManagerState::Stopped) {

			m_SceneContext.debug->LOG_INFO("シーンを停止します");

			systemRegistry->StopAll();

			TempLoad(); // 一時保存の読み込み
			OldState = State;

		} else if (State == SceneManagerState::Playing) {

			if (OldState == SceneManagerState::Stopped) {

				TempSave(); // 一時保存
				m_SceneContext.debug->LOG_INFO("シーンを開始します");
				systemRegistry->StartAll();

			} else {
				m_SceneContext.debug->LOG_INFO("シーンを再開します");
			}
			OldState = State;

		} else if (State == SceneManagerState::Step) {

			m_SceneContext.debug->LOG_INFO("シーンを1フレーム進めます");

			if (OldState == SceneManagerState::Stopped) {
				TempSave(); // 一時保存
				m_SceneContext.debug->LOG_INFO("シーンを開始します");
				systemRegistry->StartAll();
			}

			for (auto& [name, scene] : m_activeScenes) {

				scene->Update(deltaTime);
				systemRegistry->UpdateAll(deltaTime);

				scene->FixedUpdate(1.0f / TARGET_FPS);
				systemRegistry->FixedUpdateAll(1.0f / TARGET_FPS);
			}

			OldState = State;
			State = SceneManagerState::Paused;
		} else {
			OldState = State;
		}
	}

	if (State == SceneManagerState::Playing) {
		for (auto& [name, scene] : m_activeScenes) {
			scene->Update(deltaTime);
		}
		systemRegistry->UpdateAll(deltaTime);
	}
	systemRegistry->EditorUpdateAll(deltaTime);

	for (auto it = m_activeScenes.begin(); it != m_activeScenes.end(); ) {
		if (it->second->isDestroy) {

			it->second->Shutdown();
			it->second.reset();

			it = m_activeScenes.erase(it);

		} else {
			++it;
		}
	}

	if(m_NeedSceneChange){

		LoadScene(m_NextScene);

		m_NeedSceneChange = false;
		m_NextScene = nullptr;
	}
}

void SceneManager::FixedUpdate(float fixedDeltaTime){
	if (State == SceneManagerState::Playing) {
		for (auto& [name, scene] : m_activeScenes) {
			scene->FixedUpdate(fixedDeltaTime);
		}
		systemRegistry->FixedUpdateAll(fixedDeltaTime);
	}
}

void SceneManager::Draw(){
	for(auto& [name, scene] : m_activeScenes){
		scene->Draw();
	}
	systemRegistry->DrawAll();
}

void SceneManager::Shutdown(){
	for(auto& [name, scene] : m_activeScenes){
		scene->Shutdown();
		scene.reset();
	}
	m_activeScenes.clear();

	systemRegistry->FinalizeAll();
	systemRegistry.reset();
}

void SceneManager::AddScene(std::shared_ptr<Scene> scene) {
	if (!scene) {
		return;
	}

	if (m_activeScenes[scene->SceneName]) {
		m_activeScenes[scene->SceneName]->Shutdown();
		m_activeScenes[scene->SceneName] = nullptr;
	}

	m_activeScenes[scene->SceneName] = scene;
	scene->Initialize(&m_SceneContext);
}

void SceneManager::LoadScene(std::shared_ptr<Scene> scene){

	m_SceneContext.debug->LOG_INFO("Sceneを読み込みます...");
	
	for (auto& [name, scene] : m_activeScenes) {
		scene->Shutdown();
		scene.reset();
	}
	m_activeScenes.clear();

	m_activeScenes[scene->SceneName] = scene;

	scene->Initialize(&m_SceneContext);

	m_SceneContext.resource->ClearAllUnused();
}

void SceneManager::DeferredLoadScene(std::shared_ptr<Scene> scene){
	m_NextScene.reset();
	m_NeedSceneChange = true;
	m_NextScene = scene;
}

void SceneManager::SaveScenes(){
	for (auto& [name, scene] : m_activeScenes) {
		scene->Save();
	}
}

std::shared_ptr<Scene>  SceneManager::OpenFromYAMLFile(){
	auto scene = std::make_shared<Scene>();

	scene->Initialize(&m_SceneContext);
	if(scene->LoadFromYAMLFile()){

		m_SceneContext.resource->ClearAllUnused();
		return scene;
	}
	scene->Shutdown();
	scene.reset();
	return nullptr;
}

std::shared_ptr<Scene> SceneManager::LoadFromFilePath(const std::string& filePath){

	m_SceneContext.debug->LOG_INFO(("Scene[" + filePath + "]を読み込みます...").c_str());

	auto scene = std::make_shared<Scene>();
	scene->Initialize(&m_SceneContext);
	scene->ResetAll();
	scene->LoadSceneFromYAML(filePath);

	m_activeScenes[scene->SceneName] = scene;

	m_SceneContext.resource->ClearAllUnused();

	return scene;
}

void SceneManager::TempSave() {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Scenes" << YAML::Value << YAML::BeginSeq;

	for (auto& [name, scene] : m_activeScenes) {
		scene->TempSave(); // 各シーンの個別セーブ

		out << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << name;
		out << YAML::Key << "Path" << YAML::Value << scene->ScenePath;
		out << YAML::EndMap;
	}

	out << YAML::EndSeq;
	out << YAML::EndMap;

	std::ofstream fout((std::string(TEMP_SAVE_PATH) + "TempSave.yaml").c_str());
	fout << out.c_str();
	fout.close();
}

void SceneManager::TempLoad() {
	// アクティブシーンを破棄
	for (auto& [name, scene] : m_activeScenes) {
		if (scene) {
			scene->Shutdown();
			scene.reset();
		}
	}
	m_activeScenes.clear();

	// YAMLファイル読み込み
	YAML::Node data;
	try {
		data = YAML::LoadFile(std::string(TEMP_SAVE_PATH) + "TempSave.yaml");
	}
	catch (const YAML::BadFile& e) {
		m_SceneContext.debug->LOG_ERROR((std::string("TempSave.yaml が見つかりません") + e.what()).c_str());
		return;
	}

	// シーンの再ロード
	if (data["Scenes"]) {
		for (const auto& sceneNode : data["Scenes"]) {
			std::string name = sceneNode["Name"].as<std::string>();
			std::string path = sceneNode["Path"].as<std::string>();

			// Sceneの生成・ロード
			auto newScene = std::make_shared<Scene>();

			newScene->ScenePath = (std::string(TEMP_SAVE_PATH) + "Temp_" + name + ".yaml");

			newScene->Initialize(&m_SceneContext);

			newScene->SceneName = name;

			newScene->ScenePath = path;

			m_activeScenes[name] = newScene;
		}
	}

	m_SceneContext.resource->ClearAllUnused();
}
