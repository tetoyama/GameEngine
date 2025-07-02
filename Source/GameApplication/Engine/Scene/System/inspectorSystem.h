// GameEngine/Source/GameApplication/Engine/Scene/System/inspectorSystem.h
#pragma once
#include "../Interface/ISystem.h"
#include "../Entity/Entity.h" // Entityの定義をインクルード

struct SceneContext;
class TransformComponent;
class SpriteRendererComponent;
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
	void EditorUpdate(float deltaTime) override{}

private:
	SceneContext* m_context;

	TransformComponent CalculateRectTransform(
		const SpriteRendererComponent& sprite,
		const TransformComponent& originalTransform
	);
	TransformComponent ReverseCalculateRectTransform(
		const SpriteRendererComponent& sprite,
		const TransformComponent& adjustedTransform
	);
	// UIパネル描画関数
	void CreateDockSpace();
	void DrawSceneHierarchy(SceneContext* context);
	void DrawInspector(SceneContext* context);
	void DrawAssetsBrowser();

	// メンバー変数
	Entity selectedEntity = 0;

	bool* showSceneHierarchy = nullptr;
	bool* showInspector = nullptr;
	bool* showConsole = nullptr;
	bool* showAssetsBrowser = nullptr;

};