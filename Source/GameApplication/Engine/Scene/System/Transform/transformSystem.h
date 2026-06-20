// =======================================================================
// 
// transformSystem.h
// 
// =======================================================================
#pragma once
#include "Interface/ISystem.h"

struct SceneManagerContext;

// トランスフォームの描画補助を行うシステム
class TransformSystem: public ISystem {
public:
	const char* GetSystemName() const override{
		return "TransformSystem";
	}

	explicit TransformSystem(SceneManagerContext* context)
		: m_context(context){}

	void Initialize() override;
	void Draw() override;
	void RegisterTasks(SystemScheduleBuilder& builder) override;

private:
	SceneManagerContext* m_context = nullptr;
};