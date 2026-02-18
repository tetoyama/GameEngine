#pragma once
#include "Interface/ISystem.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>

struct SceneManagerContext;

class CustomScriptSystem: public ISystem {
public:

	const char* GetSystemName() const override{
		return "CustomScriptSystem";
	}

	CustomScriptSystem(SceneManagerContext* context): m_context(context){}
	~CustomScriptSystem(){}

	void Initialize() override;
	void Finalize() override;

	void Start() override;
	void Update(float deltaTime) override;
	void FixedUpdate(float fixedDeltaTime) override;
	void Draw() override;
	void EditorUpdate(float deltaTime) override;

private:
	SceneManagerContext* m_context;
};
