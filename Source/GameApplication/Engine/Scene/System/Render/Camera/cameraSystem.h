// =======================================================================
// 
// cameraSystem.h
// 
// =======================================================================
#pragma once
#include "Interface/ISystem.h"

struct SceneManagerContext;

// カメラのビュー・プロジェクション行列を更新するシステム
class CameraSystem : public ISystem {

public:
	const char* GetSystemName() const override{
		return "CameraSystem";
	}
	CameraSystem(SceneManagerContext* context): m_context(context){}

	void Initialize() override;
	void Draw() override;
	void RegisterTasks(SystemScheduleBuilder& builder) override;

private:
	SceneManagerContext* m_context;
};
