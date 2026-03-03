// =======================================================================
// 
// scene.h
// 
// =======================================================================
#pragma once
#include <memory>
#include <vector>
#include <d3d11.h>
#include <string>
#include "Backends/myVector2.h"
#include "Entity/Entity.h"

// 前方宣言
enum RenderLayer;

struct SceneManagerContext;

class EntityRegistry;
class ComponentRegistry;
class SystemRegistry;
class PrefabSystem;

struct SceneContext{

	// マネージャコンテキスト
	SceneManagerContext* manager = nullptr;

	// レジストリ
	EntityRegistry* entity = nullptr;
	ComponentRegistry* component = nullptr;
	SystemRegistry* system = nullptr;

	// プレファブシステム
	PrefabSystem* prefab = nullptr;
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

	SceneContext* GetSceneContext(){return &m_SceneContext;}

	void LoadSceneFromYAML(std::string path);
	RenderLayer GetRenderLayerFromEntity(Entity entity);

	std::string SceneName = "Untitled"; // シーンの名前
	std::string ScenePath = ""; // シーンのパス

	bool isDestroy = false; // シーンが破棄されるかどうか

private:

	std::string LoadSceneFileDialog();
	bool SaveSceneFileDialog(std::wstring& outPath);

	// TransformComponent の children リストを parent 参照から再構築する
	void RebuildTransformChildren();

	// マネージャコンテキスト
	SceneManagerContext* m_SceneManagerContext = nullptr;
	SceneContext m_SceneContext{};

	// レジストリ
	std::shared_ptr<EntityRegistry> m_entityRegistry;
	std::shared_ptr<ComponentRegistry> m_componentRegistry;
	std::shared_ptr<PrefabSystem> m_prefabSystem;
};
