// Scene/transformSystem.h
#pragma once
#include "Interface/ISystem.h"

struct SceneManagerContext;

class TransformSystem : public ISystem{
public:

	const char* GetSystemName() const override{
		return "TransformSystem";
	}
	TransformSystem(SceneManagerContext* context): m_context(context){}
	~TransformSystem(){}

	void Initialize() override;
	void Draw() override;

private:
	SceneManagerContext* m_context;
};
