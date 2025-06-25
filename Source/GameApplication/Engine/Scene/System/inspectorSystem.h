// GameEngine/Source/GameApplication/Engine/Scene/System/inspectorSystem.h
#pragma once
#include "../Interface/ISystem.h"
#include "../Entity/Entity.h" // Entityの定義をインクルード

class SceneContext; // 前方宣言

class InspectorSystem: public ISystem
{
public:
	InspectorSystem(SceneContext* context): m_context(context){}
	~InspectorSystem(){}
	void Initialize() override{}
	void Finalize()override{}

	void Start() override{}
	void Update(float deltaTime) override{}
	void FixedUpdate(float fidedDeltaTime) override{}
	void Draw() override;
private:
	SceneContext* m_context;

	// UIパネル描画関数
	void CreateDockSpace();
	void DrawSceneHierarchy(class SceneContext* context);
	void DrawInspector(class SceneContext* context);
	void DrawAssetsBrowser();

	// ヒエラルキー描画用のヘルパー
	void DrawEntityNode(class ComponentRegistry* registry, Entity entity);

	// メンバー変数
	Entity selectedEntity = 0;

	bool* showSceneHierarchy;
	bool* showInspector;
	bool* showConsole;
	bool* showAssetsBrowser;
};