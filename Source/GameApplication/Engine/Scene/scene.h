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

	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;

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
	// Scene::Initialize()はYAML decodeやResource loadを含み、途中で例外が
	// 発生し得る。明示Shutdown前にshared_ptrが巻き戻された場合でも、登録済み
	// SceneContextとRegistryを残さないよう、他の所有Memberより先に破棄される
	// GuardからShutdownを実行する。
	//
	// 正常経路ではShutdown()がm_SceneManagerContextをnullptrへ戻すためno-op。
	// 部分初期化がRegistry生成前で止まった場合はContext登録前なのでno-op。
	struct LifecycleGuard {
		Scene* owner = nullptr;

		~LifecycleGuard() noexcept {
			if(!owner || !owner->m_SceneManagerContext){
				return;
			}
			if(!owner->m_entityRegistry || !owner->m_componentRegistry){
				return;
			}
			try {
				owner->Shutdown();
			}catch(...){
				// Destructorから例外を送出しない。通常の停止経路では
				// Shutdownを明示実行するため、これは異常初期化時の最終防御。
			}
		}
	};

	std::string LoadSceneFileDialog();
	bool SaveSceneFileDialog(std::wstring& outPath);
	void RebuildTransformChildren();
	void ApplyStorageConfig();

	SceneManagerContext* m_SceneManagerContext = nullptr;
	SceneContext m_SceneContext{};

	std::unique_ptr<EntityRegistry> m_entityRegistry;
	std::unique_ptr<ComponentRegistry> m_componentRegistry;
	std::unique_ptr<PrefabSystem> m_prefabSystem;

	// Memberは宣言の逆順で破棄されるため、最後に置いて最初に実行する。
	LifecycleGuard m_lifecycleGuard{this};
};