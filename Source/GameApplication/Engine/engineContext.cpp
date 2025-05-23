#include <memory>
#include "EngineContext.h"

#include "Platform/WindowSystem/windowSystem.h"
#include "Runtime/TimeService/time.h"
#include "Graphics/GraphicsContext.h"
#include "DebugTools/ImGuiSystem.h"
#include "Platform/InputSystem/InputSystem.h"
#include "Scene/sceneManager.h"

std::shared_ptr<EngineContext> EngineContextBuilder::Build(){

	std::shared_ptr<EngineContext> context = std::make_shared<EngineContext>();

	// WindowSystem ìoò^
	auto windowSystem = std::make_shared<WindowSystem>();
	context->Register<WindowSystem>(windowSystem);

	// TimeService ìoò^
	auto timeService = std::make_shared<TimeService>();
	context->Register<TimeService>(timeService);

	// GraphicsContext ìoò^
	auto graphicsContext = std::make_shared<GraphicsContext>();
	context->Register<GraphicsContext>(graphicsContext);

	// imgui ìoò^
	auto imgui = std::make_shared<ImGuiSystem>();
	context->Register<ImGuiSystem>(imgui);

	// InputSystem ìoò^
	auto inputSystem = std::make_shared<InputSystem>();
	context->Register<InputSystem>(inputSystem);

	// SceneManager ìoò^
	auto sceneManager = std::make_shared<SceneManager>();
	context->Register<SceneManager>(sceneManager);



	// ç°å„: ëºÇÃÉVÉXÉeÉÄÇ‡Ç±Ç±Ç≈ìoò^

	return context;
}
