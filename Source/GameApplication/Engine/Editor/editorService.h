// =======================================================================
// 
// editorService.h
// 
// =======================================================================
#pragma once
#include "Service/IService.h"
#include <vector>
#include "InterFace/IEditorUI.h"
#include "Command/CommandManager.h"

struct EditorDrawContext;
class EditorService;

class ResourceService;
class DebugLogService;
class SceneManager;
class LLAMAService;

class AnalyzerManager;

struct SceneManagerContext;

struct EditorServiceContext {
	DebugLogService* debugLogSystem = nullptr;
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

	DebugLogService* debugLogSystem = nullptr;
	ResourceService* resourceService = nullptr;
	SceneManager* sceneManager = nullptr;
	LLAMAService* llamaService = nullptr;

	AnalyzerManager* analyzer = nullptr;

	CommandManager commandManager;

private:

	std::vector<IEditorUI*> UIs;
};

struct EditorDrawContext {
	double UpdateTime;		// 更新時間
	double DrawTime;		// 描画時間
	double FPS;				// 更新FPS
	double FixedUpdateFPS;	// 固定更新FPS
};
