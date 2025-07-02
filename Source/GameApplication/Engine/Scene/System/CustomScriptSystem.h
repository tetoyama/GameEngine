#pragma once
#include "Interface/ISystem.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>

struct SceneContext;

class CustomScriptSystem: public ISystem {
public:
	CustomScriptSystem(SceneContext* context): m_context(context){}
	~CustomScriptSystem(){}

	void Initialize() override{}
	void Finalize() override{}

	void Start() override;
	void Update(float deltaTime) override;
	void FixedUpdate(float fixedDeltaTime) override;
	void Draw() override;
	void EditorUpdate(float deltaTime) override;

private:
	SceneContext* m_context;
};
