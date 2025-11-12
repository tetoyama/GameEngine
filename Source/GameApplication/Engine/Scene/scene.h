// Engine/Scene/Scene.h
#pragma once
#include <memory>
#include <vector>
#include <d3d11.h>
#include <string>
#include "Backends/myVector2.h"

// 前方宣言
struct ManagerContext;

class EntityRegistry;
class ComponentRegistry;
class SystemRegistry;

enum SceneState
{
	Playing, // ゲームプレイ中
	Paused,  // 一時停止中
	Stopped, // 停止中
	Step,
};

struct SceneContext{

	// マネージャコンテキスト
	ManagerContext* manager = nullptr;

	// レジストリ
	EntityRegistry* entity = nullptr;
	ComponentRegistry* component = nullptr;
	SystemRegistry* system = nullptr;

	// シーンの状態
	SceneState state = SceneState::Stopped;

	// 画面情報
	Vector2 PlayerScreenSize = {1280.0f, 720.0f};
	Vector2 EditorScreenSize = {1280.0f, 720.0f};
};

class Scene {
public:
	Scene();
	~Scene();

	void Initialize(ManagerContext* set);
	void Update(float deltaTime);
	void FixedUpdate(float fixedDeltaTime);
	void Draw();
	void Shutdown();

	void BuildDefaultScene();

	void ResetAll();

	bool LoadFromYAMLFile();
	void Save();

	void TempSave(); // 一時保存
	void TempLoad(); // 一時読み込み

	SceneContext* GetSceneContext(){return &m_SceneContext;}
	SceneState GetState() const{return m_SceneContext.state;}
	void SetState(const SceneState& state){m_SceneContext.state = state;}

	std::string SceneName = "Untitled"; // シーンの名前
	std::string ScenePath = ""; // シーンのパス
	void LoadSceneFromYAML(std::string path);

private:

	std::string LoadSceneFileDialog();
	bool SaveSceneFileDialog(std::wstring& outPath);

	// シーンの状態
	SceneState m_OldState = SceneState::Stopped;

	// マネージャコンテキスト
	ManagerContext* m_SceneManagerContext = nullptr;
	SceneContext m_SceneContext{};

	// レジストリ
	std::shared_ptr<EntityRegistry> m_entityRegistry;
	std::shared_ptr<ComponentRegistry> m_componentRegistry;
	std::shared_ptr<SystemRegistry> m_systemRegistry;

};
