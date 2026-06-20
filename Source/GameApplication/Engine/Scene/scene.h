// =======================================================================
// 
// scene.h
// 
// =======================================================================
#pragma once
#include <memory>
#include <vector>
#include <cstdint>
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
class EntityCommandBuffer;

struct SceneContext;
using SceneContextResolver = SceneContext* (*)(void* owner, uint32_t contextID);

// シーン内の各レジストリ・マネージャへのポインタをまとめたコンテキスト
struct SceneContext{

	// マネージャコンテキスト
	SceneManagerContext* manager = nullptr;

	// SceneManagerが発行する、このSceneContext固有のID。
	// 0は未登録または無効なContextを表す。
	uint32_t contextID = 0;

	// EntityRefなどが外部モジュールからContextを解決するためのコールバック。
	// SceneManagerの非inlineメソッドを直接呼ばないため、DLL間の未解決シンボルを防ぐ。
	void* resolverOwner = nullptr;
	SceneContextResolver resolver = nullptr;

	// レジストリ
	EntityRegistry* entity = nullptr;
	ComponentRegistry* component = nullptr;
	SystemRegistry* system = nullptr;

	// Task / Script実行中の構造変更を遅延させるScene単位のCommand Buffer。
	// Earliest PhaseでEntityCommandCommitSystemが接続する。
	EntityCommandBuffer* commands = nullptr;

	// プレファブシステム
	PrefabSystem* prefab = nullptr;
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
