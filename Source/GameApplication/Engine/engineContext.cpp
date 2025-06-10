#include <memory>
#include "EngineContext.h"

#include "Platform/WindowSystem/windowSystem.h"
#include "Runtime/TimeService/time.h"
#include "Graphics/GraphicsContext.h"
#include "DebugTools/ImGuiSystem.h"
#include "DebugTools/DebugSystem.h"
#include "Platform/InputSystem/InputSystem.h"
#include "Scene/sceneManager.h"
#include "Graphics/mainRenderer.h"
#include "Resources/resourceSystem.h"

std::shared_ptr<EngineContext> EngineContextBuilder::Build(){

	std::shared_ptr<EngineContext> context = std::make_shared<EngineContext>();

	// WindowService ìoò^
	auto windowSystem = std::make_shared<WindowService>();
	context->Register<WindowService>(windowSystem);

	// TimeService ìoò^
	auto timeService = std::make_shared<TimeService>();
	context->Register<TimeService>(timeService);

	// GraphicsContext ìoò^
	auto graphicsContext = std::make_shared<GraphicsContext>();
	context->Register<GraphicsContext>(graphicsContext);

	// MainRenderer ìoò^
	auto mainRenderer = std::make_shared<MainRenderer>();
	context->Register<MainRenderer>(mainRenderer);

	// imgui ìoò^
	auto imgui = std::make_shared<ImGuiService>();
	context->Register<ImGuiService>(imgui);

	// DebugLogSystem ìoò^
	auto debugLogSystem = std::make_shared<DebugLogSystem>();
	context->Register<DebugLogSystem>(debugLogSystem);

	// InputService ìoò^
	auto inputSystem = std::make_shared<InputService>();
	context->Register<InputService>(inputSystem);

	// SceneManager ìoò^
	auto sceneManager = std::make_shared<SceneManager>();
	context->Register<SceneManager>(sceneManager);

	// ResourceService ìoò^
	auto resourceSystem = std::make_shared<ResourceService>();
	context->Register<ResourceService>(resourceSystem);

	// ç°å„: ëºÇÃÉVÉXÉeÉÄÇ‡Ç±Ç±Ç≈ìoò^

	return context;
}

void EngineContext::Shutdown() {

	// ãtèáÇ≈ShutdownåƒÇ—èoÇµ
	for (auto it = m_ServiceOrder.rbegin(); it != m_ServiceOrder.rend(); ++it) {
		auto found = m_Services.find(*it);
		if (found != m_Services.end()) {
			auto service = std::static_pointer_cast<IService>(found->second);
			if (service) service->Shutdown();
		}
	}
	m_Services.clear();
	m_ServiceOrder.clear();
}

