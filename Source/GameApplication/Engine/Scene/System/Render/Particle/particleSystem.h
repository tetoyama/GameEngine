// =======================================================================
// 
// particleSystem.h
// 
// =======================================================================
#pragma once
#include "Interface/ISystem.h"
#include "Entity/Entity.h" // Entityの定義をインクルード

struct SceneManagerContext;
class TransformComponent;
class SpriteRendererComponent;
// パーティクルの生成・更新を管理するシステム
class ParticleSystem : public ISystem {
public:

	const char* GetSystemName() const override{
		return "ParticleSystem";
	}

	ParticleSystem(SceneManagerContext* context) : m_context(context) {}
	~ParticleSystem() {}
	void Initialize() override;
	void Finalize() override;

	void Start() override;
	void Update(float deltaTime);
	void RegisterTasks(SystemScheduleBuilder& builder) override;

private:
	SceneManagerContext* m_context;
};
