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

	// WindowService 登録
	auto windowSystem = std::make_shared<WindowService>();
	context->Register<WindowService>(windowSystem);

	// TimeService 登録
	auto timeService = std::make_shared<TimeService>();
	context->Register<TimeService>(timeService);

	// GraphicsContext 登録
	auto graphicsContext = std::make_shared<GraphicsContext>();
	context->Register<GraphicsContext>(graphicsContext);

	// MainRenderer 登録
	auto mainRenderer = std::make_shared<MainRenderer>();
	context->Register<MainRenderer>(mainRenderer);

	// imgui 登録
	auto imgui = std::make_shared<ImGuiService>();
	context->Register<ImGuiService>(imgui);

	// DebugLogSystem 登録
	auto debugLogSystem = std::make_shared<DebugLogSystem>();
	context->Register<DebugLogSystem>(debugLogSystem);

	// InputService 登録
	auto inputSystem = std::make_shared<InputService>();
	context->Register<InputService>(inputSystem);

	// SceneManager 登録
	auto sceneManager = std::make_shared<SceneManager>();
	context->Register<SceneManager>(sceneManager);

	// ResourceService 登録
	auto resourceSystem = std::make_shared<ResourceService>();
	context->Register<ResourceService>(resourceSystem);

	// 今後: 他のシステムもここで登録

	return context;
}

void EngineContext::Shutdown() {

	// 逆順でShutdown呼び出し
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

