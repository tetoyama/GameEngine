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
enum m_RenderLayer;

struct SceneManagerContext;

class entityRegistry;
class componentRegistry;
class systemRegistry;
class prefabSystem;

// シーン内の各レジストリ・マネージャへのポインタをまとめたコンテキスト
struct SceneContext{

	// マネージャコンテキスト
	SceneManagerContext* pManager = nullptr;

	// レジストリ
	EntityRegistry* pEntity = nullptr;
	ComponentRegistry* pComponent = nullptr;
	SystemRegistry* pSystem = nullptr;

	// プレファブシステム
	PrefabSystem* pPrefab = nullptr;
};

// シーンの生成・読み込み・保存・破棄を行うクラス
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

	std::string sceneName= "Untitled"; // シーンの名前
	std::string scenePath= ""; // シーンのパス

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
	std::shared_ptr<EntityRegistry> m_EntityRegistry;
	std::shared_ptr<ComponentRegistry> m_ComponentRegistry;
	std::shared_ptr<PrefabSystem> m_PrefabSystem;
};
