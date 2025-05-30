#pragma once
#include <memory>
#include <windows.h>
#include "engineContext.h"

class MainRenderer;
class WindowSystem;
class TimeService;
class GraphicsContext;
class ImGuiSystem;
class InputSystem;
class SceneManager;
class Scene;
class MainWindow;

class Engine
{
public:
	void Initialize(std::shared_ptr<EngineContext> context, HINSTANCE hInstance, int nCmdShow);
	void Shutdown(std::shared_ptr<EngineContext> context);

	void Run(std::shared_ptr<EngineContext> context);

private:


};
