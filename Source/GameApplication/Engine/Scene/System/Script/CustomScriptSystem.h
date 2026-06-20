// =======================================================================
//
// CustomScriptSystem.h
//
// =======================================================================
#pragma once

#include "Interface/ISystem.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct SceneManagerContext;
class CustomScriptComponent;

class CustomScriptSystem: public ISystem {
public:
	const char* GetSystemName() const override{
		return "CustomScriptSystem";
	}

	CustomScriptSystem(SceneManagerContext* context): m_context(context){}
	~CustomScriptSystem() override = default;

	void Initialize() override;
	void Finalize() override;

	void Start() override;
	void Update(float deltaTime) override;
	void FixedUpdate(float fixedDeltaTime) override;
	void Draw() override;
	void EditorUpdate(float deltaTime) override;

private:
	void ForEachScriptOrdered(
		SystemTaskDomain domain,
		const std::function<void(CustomScriptComponent*)>& callback
	);

	SceneManagerContext* m_context = nullptr;
};
