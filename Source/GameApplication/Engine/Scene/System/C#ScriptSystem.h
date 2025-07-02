// Engine/Scene/ScriptSystem.h
#pragma once
#include "Interface/ISystem.h"

struct SceneContext;

class CSharpScriptSystem: public ISystem{
public:
	CSharpScriptSystem(SceneContext* context): m_context(context){}
	~CSharpScriptSystem(){}

	void Initialize() override{}
	void Finalize()override{};

	void Start() override;
	void Update(float deltaTime) override;
	void FixedUpdate(float fidedDeltaTime) override;
	void Draw() override;
	void EditorUpdate(float deltaTime) override;
private:
	SceneContext* m_context;
};
