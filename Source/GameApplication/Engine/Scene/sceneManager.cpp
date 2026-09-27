// =======================================================================
// 
// sceneManager.cpp
// 
// =======================================================================
#include <memory>
#include <algorithm>
#include <exception>
#include <filesystem>

#include "buildSetting.h"
#include "gameApplication.h"

#include "sceneManager.h"
#include "scene.h"

#include "DebugTools/debugSystem.h"
#include "Resources/resourceService.h"

#include "Registry/systemRegistry.h"

#include "Service/Config/configSystem.h"

#include "System/Transform/transformSystem.h"
#include "System/Transform/followSystem.h"

#include "System/Render/RenderSystem/renderSystem.h"
#include "System/Render/StaticBatch/StaticBatchUploadSystem.h"
#include "System/Render/Camera/CameraSystem.h"
#include "System/Render/Culling/CullingSystem.h"
#include "System/Render/Effect/effectSystem.h"
#include "System/Render/Terrain/waveSystem.h"
#include "System/Render/Terrain/terrainSystem.h"
#include "System/Render/Particle/particleSystem.h"

#include "System/Script/CustomScriptSystem.h"

#include "System/Audio/audioSystem.h"

#include "System/Physic/physicSystem.h"
#include "System/Script/ScriptSystem.h"

#include "Editor/editorService.h"
#include <Editor/UI/Inspector.h>
#include <Editor/UI/Hierarchy.h>

SceneManager::SceneManager() = default;

SceneManager::~SceneManager() = default;

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
	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_INFO("SceneManager の初期化を開始します");
	}

	m_systemRegistry = std::make_unique<SystemRegistry>();

	m_systemRegistry->RegisterSystem(std::make_unique<ScriptSystem>(&m_SceneContext));

	m_systemRegistry->RegisterSystem(std::make_unique<TransformSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<FollowSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<CameraSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<CullingSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<RenderSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<StaticBatchUploadSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<AudioSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<ParticleSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<EffectSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<TerrainSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<PhysicSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<CustomScriptSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<WaveSystem>(&m_SceneContext));

	// システムの初期化
	m_systemRegistry->InitializeAll();
	m_systemRegistry->DecodeAll(m_SceneContext.config->editorConfig);

	m_SceneContext.systemRegistry = m_systemRegistry.get();

	for (auto& [name, scene] : m_activeScenes) {
		scene->Initialize(&m_SceneContext);
	}

	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_INFO("SceneManager の初期化が完了しました");
	}
}

void SceneManager::Update(float deltaTime){

	if (OldState != State) {
		if (State == SceneManagerState::Paused) {

			if(m_SceneContext.debug){
				m_SceneContext.debug->LOG_INFO("シーンを一時停止します");
			}
			OldState = State;

		} else if (State == SceneManagerState::Stopped) {

			if(m_SceneContext.debug){
				m_SceneContext.debug->LOG_INFO("シーンを停止します");
			}

			m_systemRegistry->StopAll();

			TempLoad(); // 一時保存の読み込み
			// プレイ中に積まれたコマンドは TempLoad 後に無効なため全クリア
			if (m_SceneContext.editor) {
				m_SceneContext.editor->commandManager.Clear();
			}

			OldState = State;

		} else if (State == SceneManagerState::Playing) {

			if (OldState == SceneManagerState::Stopped) {

				TempSave(); // 一時保存
				// プレイ開始前のエディタ操作コマンドをクリア（プレイ中は Undo/Redo 無効）
				if (m_SceneContext.editor) {
					m_SceneContext.editor->commandManager.Clear();
				}

				if(m_SceneContext.debug){
					m_SceneContext.debug->LOG_INFO("シーンを開始します");
				}
				m_systemRegistry->StartAll();

			} else {
				if(m_SceneContext.debug){
					m_SceneContext.debug->LOG_INFO("シーンを再開します");
				}
			}
			OldState = State;

		} else if (State == SceneManagerState::Step) {

			if(m_SceneContext.debug){
				m_SceneContext.debug->LOG_INFO("シーンを1フレーム進めます");
			}

			if (OldState == SceneManagerState::Stopped) {
				TempSave(); // 一時保存
				if (m_SceneContext.editor) {
					m_SceneContext.editor->commandManager.Clear();
				}
				if(m_SceneContext.debug){
					m_SceneContext.debug->LOG_INFO("シーンを開始します");
				}
				m_systemRegistry->StartAll();
			}

			// Scene固有処理は各Sceneに対して実行する。
			for (auto& [name, scene] : m_activeScenes) {
				scene->Update(deltaTime);
			}

			// System側が全Active Sceneを処理するため、Sceneループ外で一度だけ実行する。
			m_systemRegistry->UpdateAll(deltaTime);

			const float fixedDeltaTime = 1.0f / TARGET_FPS;
			for (auto& [name, scene] : m_activeScenes) {
				scene->FixedUpdate(fixedDeltaTime);
			}

			// FixedUpdate系Systemも1ステップにつき一度だけ実行する。
			m_systemRegistry->FixedUpdateAll(fixedDeltaTime);

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
		m_systemRegistry->UpdateAll(deltaTime);
	}
	m_systemRegistry->EditorUpdateAll(deltaTime);

	for (auto it = m_activeScenes.begin(); it != m_activeScenes.end(); ) {
		if (it->second->isDestroy) {

			// シーン破棄前に CommandManager をクリアする。
			// シーンの SceneContext* やコンポーネントポインタを保持するコマンドが
			// Shutdown 後に Undo/Redo されてダングリングポインタ参照を起こすことを防ぐ。
			if (m_SceneContext.editor) {
				m_SceneContext.editor->commandManager.Clear();
			}

			it->second->Shutdown();
			it->second.reset();
			if(m_SceneContext.debug){
				m_SceneContext.debug->LOG_INFO(("シーンを破棄しました: " + it->first).c_str());
			}

			it = m_activeScenes.erase(it);

		} else {
			++it;
		}
	}

	if(m_NeedSceneChange){
		if(m_SceneContext.debug && m_NextScene){
			m_SceneContext.debug->LOG_INFO(("遅延シーン切り替えを適用します: " + m_NextScene->SceneName).c_str());
		}

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
		m_systemRegistry->FixedUpdateAll(fixedDeltaTime);
	}
}

void SceneManager::Draw(){
	for(auto& [name, scene] : m_activeScenes){
		scene->Draw();
	}
	m_systemRegistry->DrawAll();
}

void SceneManager::Shutdown(){
	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_INFO("SceneManager を終了します");
	}

	for(auto& [name, scene] : m_activeScenes){
		if(scene){
			scene->Shutdown();
			scene.reset();
		}
	}
	m_activeScenes.clear();

	// Scene::Shutdown() が個別登録を解除するが、異常終了経路で残った
	// Contextもここで確実に無効化する。
	m_contextRegistry.clear();

	if(m_systemRegistry){
		m_systemRegistry->EncodeAll(m_SceneContext.config->editorConfig);
		m_systemRegistry->FinalizeAll();
		m_systemRegistry.reset();
	}
	m_SceneContext.systemRegistry = nullptr;
}

uint32_t SceneManager::GetIDFromContext(SceneContext* ctx){
	if(ctx == nullptr) return 0;

	// SceneContext自身が保持するIDを使い、線形探索を避ける。
	if(ctx->contextID != 0){
		auto it = m_contextRegistry.find(ctx->contextID);
		if(it != m_contextRegistry.end() && it->second == ctx){
			return ctx->contextID;
		}

		// Context側だけに古いIDが残っていた場合は再登録する。
		ctx->contextID = 0;
	}

	const uint32_t newID = m_nextContextID++;
	m_contextRegistry[newID] = ctx;
	ctx->contextID = newID;
	return newID;
}

SceneContext* SceneManager::GetContextFromID(uint32_t id){
	if(id == 0) return nullptr;

	auto it = m_contextRegistry.find(id);
	return it != m_contextRegistry.end() ? it->second : nullptr;
}

void SceneManager::UnregisterContext(SceneContext* ctx){
	if(ctx == nullptr) return;

	if(ctx->contextID != 0){
		auto it = m_contextRegistry.find(ctx->contextID);
		if(it != m_contextRegistry.end() && it->second == ctx){
			m_contextRegistry.erase(it);
		}
		ctx->contextID = 0;
		return;
	}

	// 旧データや不整合状態に対するフォールバック。
	for(auto it = m_contextRegistry.begin(); it != m_contextRegistry.end(); ++it){
		if(it->second == ctx){
			m_contextRegistry.erase(it);
			break;
		}
	}
}

void SceneManager::AddScene(std::shared_ptr<Scene> scene) {
	if (!scene) {
		if(m_SceneContext.debug){
			m_SceneContext.debug->LOG_WARNING("追加対象のシーンが nullptr です");
		}
		return;
	}

	auto existing = m_activeScenes.find(scene->SceneName);
	if (existing != m_activeScenes.end() && existing->second) {
		// 既存シーンを上書きする際は CommandManager をクリアする。
		// 古いシーンのポインタを保持するコマンドが残ると Undo/Redo でクラッシュする。
		if (m_SceneContext.editor) {
			m_SceneContext.editor->commandManager.Clear();
		}
		existing->second->Shutdown();
	}

	m_activeScenes[scene->SceneName] = scene;
	scene->Initialize(&m_SceneContext);
	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_INFO(("シーンを追加しました: " + scene->SceneName).c_str());
	}
}

void SceneManager::LoadScene(std::shared_ptr<Scene> scene){
	if(!scene){
		if(m_SceneContext.debug){
			m_SceneContext.debug->LOG_ERROR("LoadScene に nullptr が渡されました");
		}
		return;
	}

	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_INFO("Sceneを読み込みます...");
	}

	// シーン全置換前に CommandManager をクリアする（ダングリングポインタ防止）
	if (m_SceneContext.editor) {
		m_SceneContext.editor->commandManager.Clear();
	}
	
	for (auto& [name, scene] : m_activeScenes) {
		scene->Shutdown();
		scene.reset();
	}
	m_activeScenes.clear();

	m_activeScenes[scene->SceneName] = scene;

	scene->Initialize(&m_SceneContext);
	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_INFO(("シーンの読み込みが完了しました: " + scene->SceneName).c_str());
	}

	m_SceneContext.resource->ClearAllUnused();
}

void SceneManager::DeferredLoadScene(std::shared_ptr<Scene> scene){
	m_NextScene.reset();
	m_NeedSceneChange = true;
	m_NextScene = scene;
	if(m_SceneContext.debug && scene){
		m_SceneContext.debug->LOG_DEBUG(("シーン切り替えを予約しました: " + scene->SceneName).c_str());
	}
}

void SceneManager::SaveScenes(){
	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_INFO("アクティブシーンの保存を開始します");
	}
	for (auto& [name, scene] : m_activeScenes) {
		scene->Save();
	}
	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_INFO("アクティブシーンの保存が完了しました");
	}
}

std::shared_ptr<Scene>  SceneManager::OpenFromYAMLFile(){
	auto scene = std::make_shared<Scene>();

	scene->Initialize(&m_SceneContext);
	if(scene->LoadFromYAMLFile()){
		if(m_SceneContext.debug){
			m_SceneContext.debug->LOG_INFO(("YAML からシーンを読み込みました: " + scene->SceneName).c_str());
		}
		m_SceneContext.resource->ClearAllUnused();
		return scene;
	}
	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_WARNING("YAML シーンの読み込みに失敗しました");
	}
	scene->Shutdown();
	scene.reset();
	return nullptr;
}

std::shared_ptr<Scene> SceneManager::LoadFromFilePath(const std::string& filePath){

	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_INFO(("Scene[" + filePath + "]を読み込みます...").c_str());
	}

	auto scene = std::make_shared<Scene>();
	// Initializeより前にPathを設定し、Storage設定の先読みと
	// Default Scene生成を挟まない直接ロードを成立させる。
	scene->ScenePath = filePath;
	scene->Initialize(&m_SceneContext);

	m_activeScenes[scene->SceneName] = scene;
	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_INFO(("ファイルパスからシーンを読み込みました: " + scene->SceneName).c_str());
	}

	m_SceneContext.resource->ClearAllUnused();

	return scene;
}

void SceneManager::TempSave() {
	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_TRACE("シーンの一時保存を開始します");
	}
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
	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_TRACE("シーンの一時保存が完了しました");
	}
}

void SceneManager::TempLoad() {
	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_TRACE("シーンの一時復元を開始します");
	}

	const auto logRestoreError = [this](const std::string& message){
		if(m_SceneContext.debug){
			m_SceneContext.debug->LOG_ERROR(message.c_str());
		}
	};

	// Manifestの読み込みに失敗しても、現在のSceneを破棄しない。
	const std::string manifestPath =
		std::string(TEMP_SAVE_PATH) + "TempSave.yaml";
	YAML::Node data;
	try {
		data = YAML::LoadFile(manifestPath);
	}catch(const YAML::Exception& exception){
		logRestoreError(
			"TempSave manifestの読み込みに失敗しました: " +
			std::string(exception.what())
		);
		return;
	}catch(const std::exception& exception){
		logRestoreError(
			"TempSave manifestの復元準備に失敗しました: " +
			std::string(exception.what())
		);
		return;
	}

	YAML::Node sceneNodes;
	try {
		sceneNodes = data["Scenes"];
		if(!sceneNodes || !sceneNodes.IsSequence()){
			logRestoreError(
				"TempSave manifestのScenesが存在しないかSequenceではありません"
			);
			return;
		}
	}catch(const YAML::Exception& exception){
		logRestoreError(
			"TempSave manifestのScenes解析に失敗しました: " +
			std::string(exception.what())
		);
		return;
	}

	// 旧Sceneを保持したまま、復元先Sceneを全件構築する。
	// 途中で失敗した場合、stagedScenesの破棄時にSceneのLifecycleGuardが
	// 部分初期化済みRegistry / ContextをShutdownし、現在Sceneは維持される。
	std::unordered_map<std::string, std::shared_ptr<Scene>> stagedScenes;
	stagedScenes.reserve(sceneNodes.size());

	try {
		for(const auto& sceneNode : sceneNodes){
			if(!sceneNode.IsMap()){
				logRestoreError(
					"TempSave manifestにMapではないScene Entryがあります"
				);
				return;
			}

			const std::string name =
				sceneNode["Name"].as<std::string>();
			const std::string path =
				sceneNode["Path"].as<std::string>();
			if(name.empty()){
				logRestoreError(
					"TempSave manifestに空のScene名があります"
				);
				return;
			}
			if(stagedScenes.contains(name)){
				logRestoreError(
					"TempSave manifestに重複Scene名があります: " + name
				);
				return;
			}

			const std::string tempScenePath =
				std::string(TEMP_SAVE_PATH) + "Temp_" + name + ".yaml";
			if(!std::filesystem::exists(tempScenePath)){
				logRestoreError(
					"一時Sceneファイルが存在しません: " + tempScenePath
				);
				return;
			}

			// Initialize後に空Sceneへ差し替わることを防ぐため、Root構造を
			// 先に検証する。Component decode自体の例外はInitializeから捕捉する。
			YAML::Node tempSceneRoot = YAML::LoadFile(tempScenePath);
			YAML::Node entities = tempSceneRoot["Entities"];
			if(!entities || !entities.IsSequence()){
				logRestoreError(
					"一時SceneのEntitiesが存在しないかSequenceではありません: " +
					tempScenePath
				);
				return;
			}

			auto newScene = std::make_shared<Scene>();
			newScene->ScenePath = tempScenePath;
			newScene->Initialize(&m_SceneContext);
			newScene->SceneName = name;
			newScene->ScenePath = path;
			stagedScenes.emplace(name, std::move(newScene));
		}
	}catch(const YAML::Exception& exception){
		logRestoreError(
			"一時Scene YAMLの復元に失敗しました: " +
			std::string(exception.what())
		);
		return;
	}catch(const std::exception& exception){
		logRestoreError(
			"一時Sceneの初期化に失敗しました: " +
			std::string(exception.what())
		);
		return;
	}catch(...){
		logRestoreError(
			"一時Sceneの初期化中に不明な例外が発生しました"
		);
		return;
	}

	// すべての復元先Sceneが構築できた後だけAtomicに所有Mapを交換する。
	// swap後、stagedScenesは旧Sceneを保持するため、active Sceneが空になる
	// 中間状態を作らずに旧SceneをShutdownできる。
	if(m_SceneContext.editor){
		m_SceneContext.editor->commandManager.Clear();
	}
	m_activeScenes.swap(stagedScenes);

	for(auto& [name, oldScene] : stagedScenes){
		if(!oldScene){
			continue;
		}
		try {
			oldScene->Shutdown();
		}catch(const std::exception& exception){
			logRestoreError(
				"旧Sceneの終了処理に失敗しました[" + name + "]: " +
				std::string(exception.what())
			);
		}catch(...){
			logRestoreError(
				"旧Sceneの終了処理中に不明な例外が発生しました[" +
				name + "]"
			);
		}
		oldScene.reset();
	}
	stagedScenes.clear();

	if(m_SceneContext.resource){
		m_SceneContext.resource->ClearAllUnused();
	}
	if(m_SceneContext.debug){
		m_SceneContext.debug->LOG_TRACE("シーンの一時復元が完了しました");
	}
}
