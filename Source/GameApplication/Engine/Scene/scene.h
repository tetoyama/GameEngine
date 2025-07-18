// Engine/Scene/Scene.h
#pragma once
#include <memory>
#include <vector>
#include <d3d11.h>
#include <string>
#include "Backends/myVector2.h"

struct ManagerContext;

class EntityRegistry;
class ComponentRegistry;
class SystemRegistry;

enum SceneState
{
	Playing, // ゲームプレイ中
	Paused,  // 一時停止中
	Stopped  // 停止中
};

struct SceneContext{
	EntityRegistry* entity = nullptr;
	ComponentRegistry* component = nullptr;
	SystemRegistry* system = nullptr;

	ManagerContext* manager = nullptr;

	SceneState state = SceneState::Stopped;

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

	SceneState GetState() const{
		return m_SceneContext.state;
	}
	void SetState(const SceneState& state){
		m_SceneContext.state = state;
	}

	std::string ScenePath = ""; // シーンのパス

private:
	void LoadSceneFromYAML(std::string path);
	std::string LoadSceneFileDialog();
	bool SaveSceneFileDialog(std::wstring& outPath);

	std::shared_ptr<EntityRegistry> m_entityRegistry;
	std::shared_ptr<ComponentRegistry> m_componentRegistry;
	std::shared_ptr<SystemRegistry> m_systemRegistry;

	ManagerContext* m_SceneManagerContext = nullptr;

	SceneState m_OldState = SceneState::Stopped;

	SceneContext m_SceneContext{};
};
