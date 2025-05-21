#include <memory>
#include "EngineContext.h"

#include "Platform/WindowSystem/windowSystem.h"
#include "Runtime/TimeService/time.h"
#include "Graphics/GraphicsContext.h"
#include "DebugTools/ImGuiSystem.h"
#include "Platform/InputSystem/InputSystem.h"
#include "Scene/sceneManager.h"

std::shared_ptr<EngineContext> EngineContextBuilder::Build(HINSTANCE hInstance, int nCmdShow){

	std::shared_ptr<EngineContext> context = std::make_shared<EngineContext>();

	// WindowSystem ìoò^
	auto windowSystem = std::make_shared<WindowSystem>();
	if(!windowSystem->Initialize(hInstance, nCmdShow)){
		// ÉçÉOèoóÕÇ‚ÉGÉâÅ[èàóùÇÇ±Ç±Ç…í«â¡ó\íË
		return nullptr;
	}
	context->Register<WindowSystem>(windowSystem);

	// TimeService ìoò^
	auto timeService = std::make_shared<TimeService>();
	timeService->Initialize();
	context->Register<TimeService>(timeService);

	// GraphicsContext ìoò^
	auto graphicsContext = std::make_shared<GraphicsContext>();
	if(!graphicsContext->Initialize(windowSystem->GetMainWindow()->GetHWND(), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT)){
		// ÉçÉOèoóÕÇ‚ÉGÉâÅ[èàóùÇÇ±Ç±Ç…í«â¡ó\íË
		return nullptr;
	}
	context->Register<GraphicsContext>(graphicsContext);

	// imgui ìoò^
	auto imgui = std::make_shared<ImGuiSystem>();
	imgui->Initialize(windowSystem->GetMainWindow().get(), graphicsContext.get());
	context->Register<ImGuiSystem>(imgui);

	// InputSystem ìoò^
	auto inputSystem = std::make_shared<InputSystem>();
	inputSystem->Initialize(windowSystem->GetMainWindow()->GetHWND());
	context->Register<InputSystem>(inputSystem);

	// SceneManager ìoò^
	auto sceneManager = std::make_shared<SceneManager>();
	sceneManager->Initialize();
	context->Register<SceneManager>(sceneManager);


	// ç°å„: ëºÇÃÉVÉXÉeÉÄÇ‡Ç±Ç±Ç≈ìoò^

	return context;
}
