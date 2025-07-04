// GameEngine/Source/GameApplication/Engine/Scene/System/inspectorSystem.h
#pragma once
#include "../Interface/ISystem.h"
#include "../Entity/Entity.h" // Entityの定義をインクルード

struct SceneContext;
class TransformComponent;
class SpriteRendererComponent;
class ParticleSystem : public ISystem {
public:
	ParticleSystem(SceneContext* context) : m_context(context) {}
	~ParticleSystem() {}
	void Initialize() override;
	void Finalize()override;

	void Start() override;
	void Update(float deltaTime) override {}
	void FixedUpdate(float fidedDeltaTime) override {}
	void Draw() override {}
	void EditorUpdate(float deltaTime) override {}

private:
	SceneContext* m_context;
};