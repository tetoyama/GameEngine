#pragma once
#include "Service/IService.h"
#include <vector>
#include "InterFace/IEditorUI.h"

struct EditorDrawContext;
class EditorService;

class ResourceService;
class DebugLogSystem;
class SceneManager;
class LLAMAService;

struct SceneManagerContext;

struct EditorServiceContext {
	DebugLogSystem* debugLogSystem = nullptr;
	ResourceService* resourceService = nullptr;
	SceneManager* sceneManager = nullptr;
	LLAMAService* llamaService = nullptr;
};

class EditorService : public IService {

public:

	void Initialize(EditorServiceContext context);
	void Draw(EditorDrawContext ctx);
	void Shutdown() override;

	template<typename T>
	T* GetUI() {
		static_assert(std::is_base_of<IEditorUI, T>::value, "T must inherit from IEditorUI");
		for (auto& ui : UIs) {
			if (auto p = dynamic_cast<T*>(ui)) {
				return p;
			}
		}
		return nullptr;
	}

	DebugLogSystem* debugLogSystem = nullptr;
	ResourceService* resourceService = nullptr;
	SceneManager* sceneManager = nullptr;
	LLAMAService* llamaService = nullptr;

private:

	std::vector<IEditorUI*> UIs;
};

struct EditorDrawContext {
	double UpdateTime;
	double DrawTime;
	double FPS;
	double DeltaFPS;
};
