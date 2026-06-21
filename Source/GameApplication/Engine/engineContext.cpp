// =======================================================================
//
// engineContext.cpp
//
// =======================================================================

#include <memory>
#include "engineContext.h"

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
#include "Editor/editorService.h"
#include "LlamaService/LLAMAService.h"

std::unique_ptr<EngineContext> EngineContextBuilder::Build(){
	std::unique_ptr<EngineContext> context = std::make_unique<EngineContext>();

	auto debugLogOwner = std::make_unique<DebugLogService>();
	DebugLogService* debugLogSystem = debugLogOwner.get();
	context->Register<DebugLogService>(std::move(debugLogOwner));

	context->Register<ConfigService>(std::make_unique<ConfigService>());
	context->Register<WindowService>(
		std::make_unique<WindowService>(debugLogSystem)
	);
	context->Register<TimeService>(std::make_unique<TimeService>());
	context->Register<AudioContext>(
		std::make_unique<AudioContext>(debugLogSystem)
	);
	context->Register<GraphicsContext>(
		std::make_unique<GraphicsContext>(debugLogSystem)
	);
	context->Register<MainRenderer>(std::make_unique<MainRenderer>());
	context->Register<InputService>(std::make_unique<InputService>());
	context->Register<ResourceService>(std::make_unique<ResourceService>());
	context->Register<SceneManager>(std::make_unique<SceneManager>());
	context->Register<ImGuiService>(std::make_unique<ImGuiService>());

#ifdef _EDITOR
	context->Register<EditorService>(std::make_unique<EditorService>());
#endif

	context->Register<LLAMAService>(
		std::make_unique<LLAMAService>(debugLogSystem)
	);

	return context;
}

void EngineContext::Shutdown(){
	for(auto iterator = m_serviceOrder.rbegin();
		iterator != m_serviceOrder.rend();
		++iterator){
		auto found = m_services.find(*iterator);
		if(found != m_services.end() && found->second){
			found->second->Shutdown();
		}
	}
	m_services.clear();
	m_serviceOrder.clear();
}
