// =======================================================================
// 
// transformSystem.h
// 
// =======================================================================
#pragma once
#include "Interface/ISystem.h"

struct SceneManagerContext;

// トランスフォームの描画補助を行うシステム
class TransformSystem : public isystem{
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
