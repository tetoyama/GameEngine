#pragma once
#include "Service/IService.h"
#include <vector>
#include "InterFace/IEditorUI.h"

struct EditorDrawContext;
class EditorService;

class ResourceService;
class DebugLogSystem;
class SceneManager;
struct SceneManagerContext;

class EditorService : public IService {

public:

	void Initialize(DebugLogSystem* debug, ResourceService* resource, SceneManager* manager);
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

private:

	std::vector<IEditorUI*> UIs;
};

struct EditorDrawContext {
	double UpdateTime;
	double DrawTime;
	double FPS;
	double DeltaFPS;
};
