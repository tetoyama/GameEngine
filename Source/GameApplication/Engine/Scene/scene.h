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
#include "System/Script/ScriptModuleAPI.h"
#include "Config/SceneStorageConfig.h"

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
	SceneManagerContext* manager = nullptr;
	uint32_t contextID = 0;

	void* resolverOwner = nullptr;
	SceneContextResolver resolver = nullptr;

	EntityRegistry* entity = nullptr;
	ComponentRegistry* component = nullptr;
	SystemRegistry* system = nullptr;

	// Scene自身が所有するStorage初期確保設定。
	SceneStorageConfig storageConfig{};

	EntityCommandBuffer* commands = nullptr;
	ScriptCommandAPI scriptCommands{};
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
	void TempSave();

	SceneContext* GetSceneContext(){return &m_SceneContext;}
	SceneStorageConfig& GetStorageConfig() noexcept {
		return m_SceneContext.storageConfig;
	}
	const SceneStorageConfig& GetStorageConfig() const noexcept {
		return m_SceneContext.storageConfig;
	}

	void LoadSceneFromYAML(std::string path);
	RenderLayer GetRenderLayerFromEntity(Entity entity);

	std::string SceneName = "Untitled";
	std::string ScenePath = "";
	bool isDestroy = false;

private:
	std::string LoadSceneFileDialog();
	bool SaveSceneFileDialog(std::wstring& outPath);
	void RebuildTransformChildren();
	void ApplyStorageConfig();

	SceneManagerContext* m_SceneManagerContext = nullptr;
	SceneContext m_SceneContext{};

	std::unique_ptr<EntityRegistry> m_entityRegistry;
	std::unique_ptr<ComponentRegistry> m_componentRegistry;
	std::unique_ptr<PrefabSystem> m_prefabSystem;
};
