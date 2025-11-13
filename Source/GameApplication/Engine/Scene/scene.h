// Engine/Scene/Scene.h
#pragma once
#include <memory>
#include <vector>
#include <d3d11.h>
#include <string>
#include "Backends/myVector2.h"

// 前方宣言
struct SceneManagerContext;

class EntityRegistry;
class ComponentRegistry;
class SystemRegistry;

struct SceneContext{

	// マネージャコンテキスト
	SceneManagerContext* manager = nullptr;

	// レジストリ
	EntityRegistry* entity = nullptr;
	ComponentRegistry* component = nullptr;
	SystemRegistry* system = nullptr;
};

class Scene {
public:
	Scene();
	~Scene();

	void Initialize(SceneManagerContext* set);
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

	void LoadSceneFromYAML(std::string path);

	std::string SceneName = "Untitled"; // シーンの名前
	std::string ScenePath = ""; // シーンのパス

	bool isDestroy = false; // シーンが破棄されるかどうか

private:

	std::string LoadSceneFileDialog();
	bool SaveSceneFileDialog(std::wstring& outPath);

	// マネージャコンテキスト
	SceneManagerContext* m_SceneManagerContext = nullptr;
	SceneContext m_SceneContext{};

	// レジストリ
	std::shared_ptr<EntityRegistry> m_entityRegistry;
	std::shared_ptr<ComponentRegistry> m_componentRegistry;
};
