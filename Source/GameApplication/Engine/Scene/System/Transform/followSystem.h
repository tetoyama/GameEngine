// =======================================================================
// 
// followSystem.h
// 
// =======================================================================
#pragma once
#include "Interface/ISystem.h"

struct SceneManagerContext;

// FollowComponent を持つエンティティのトランスフォームを更新するシステム
class FollowSystem : public ISystem {
public:
	const char* GetSystemName() const override { return "FollowSystem"; }

	FollowSystem(SceneManagerContext* context) : m_context(context) {}
	~FollowSystem() {}

	void Initialize() override;
	void Update(float deltaTime);
	void EditorUpdate(float deltaTime);
	void RegisterTasks(SystemScheduleBuilder& builder) override;

private:
	void ProcessFollow();

	SceneManagerContext* m_context;
};
