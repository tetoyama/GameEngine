// =======================================================================
// 
// cameraSystem.h
// 
// =======================================================================
#pragma once
#include "Interface/ISystem.h"

struct SceneManagerContext;

class CameraSystem : public ISystem {

public:
	const char* GetSystemName() const override{
		return "CameraSystem";
	}
	CameraSystem(SceneManagerContext* context): m_context(context){}

	void Initialize() override;
	void Draw() override;

private:
	SceneManagerContext* m_context;
};
