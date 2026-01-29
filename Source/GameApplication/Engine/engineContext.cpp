

#include <memory>
#include "EngineContext.h"

#include "Platform/WindowSystem/windowSystem.h"
#include "Runtime/TimeService/timeService.h"
#include "Graphics/GraphicsContext.h"
#include "DebugTools/ImGuiSystem.h"
#include "DebugTools/DebugSystem.h"
#include "Platform/InputSystem/InputSystem.h"
#include "Scene/sceneManager.h"
#include "Graphics/mainRenderer.h"
#include "Resources/resourceService.h"
#include "Audio/audioContext.h"
#include "Config/ConfigSystem.h"
#include "Engine/Editor/editorService.h"
#include "LlamaService/LLAMAService.h"

std::shared_ptr<EngineContext> EngineContextBuilder::Build(){

	std::shared_ptr<EngineContext> context = std::make_shared<EngineContext>();

	// ConfigSystem 登録
	auto configSystem = std::make_shared<ConfigSystem>();
	context->Register<ConfigSystem>(configSystem);

	// WindowService 登録
	auto windowSystem = std::make_shared<WindowService>();
	context->Register<WindowService>(windowSystem);

	// TimeService 登録
	auto timeService = std::make_shared<TimeService>();
	context->Register<TimeService>(timeService);

	// AudioContext 登録
	auto audioContext = std::make_shared<AudioContext>();
	context->Register<AudioContext>(audioContext);

	// GraphicsContext 登録
	auto graphicsContext = std::make_shared<GraphicsContext>();
	context->Register<GraphicsContext>(graphicsContext);

	// MainRenderer 登録
	auto mainRenderer = std::make_shared<MainRenderer>();
	context->Register<MainRenderer>(mainRenderer);

	// InputService 登録
	auto inputSystem = std::make_shared<InputService>();
	context->Register<InputService>(inputSystem);

	// ResourceService 登録
	auto resourceSystem = std::make_shared<ResourceService>();
	context->Register<ResourceService>(resourceSystem);

	//SceneManager 登録
	auto sceneManager = std::make_shared<SceneManager>();
	context->Register<SceneManager>(sceneManager);

	// DebugLogSystem 登録
	auto debugLogSystem = std::make_shared<DebugLogSystem>();
	context->Register<DebugLogSystem>(debugLogSystem);

	// imgui 登録
	auto imgui = std::make_shared<ImGuiService>();
	context->Register<ImGuiService>(imgui);

#ifdef _EDITOR
	// EditorService 登録
	auto editorService = std::make_shared<EditorService>();
	context->Register<EditorService>(editorService);
#endif // _EDITOR

	// LLAMAService 登録
	auto llamaService = std::make_shared<LLAMAService>();
	context->Register<LLAMAService>(llamaService);

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

