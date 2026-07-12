// =======================================================================
// 
// gameApplication.cpp
// 
// =======================================================================
#include <windows.h>
#include "gameApplication.h"

#include "engine.h"
#include "engineContext.h"
#include "Config/ConfigSystem.h"
#include "Scene/sceneManager.h"

int GameApplication::Run(HINSTANCE hInstance, int nCmdShow){
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if(FAILED(hr)){
		OutputDebugStringA("CoInitializeEx failed\n");
		return -1;
	}

	int exitCode = 0;

	EngineContextBuilder builder;
	std::unique_ptr<EngineContext> context = builder.Build();
	if(!context){
		CoUninitialize();
		return -1;
	}

	Engine engine;
	if(engine.Initialize(context.get(), hInstance, nCmdShow)){
		auto scenes = context->Get<SceneManager>();
		auto config = context->Get<ConfigService>();
		if(!scenes || !config){
			exitCode = -1;
			OutputDebugStringA(
				"GameApplication could not resolve SceneManager or ConfigService\n"
			);
		}else{
			// ApplicationConfigで指定されたEntry Sceneだけを起動する。
			// Multi-Scene構成の組み立てはEntry Scene自身のRuntimeへ委譲する。
			if(scenes->GetActiveScenes().empty() &&
			   !scenes->LoadFromFilePath(config->appConfig.startSceneFilePath)){
				exitCode = -1;
				OutputDebugStringA(
					"GameApplication failed to load the configured entry scene\n"
				);
			}else{
				scenes->State = SceneManagerState::Playing;
				engine.Run(context.get());
			}
		}
	}
	else{
		exitCode = -1;
		OutputDebugStringA("GameApplication::Run aborted because Engine::Initialize failed\n");
	}

	engine.Shutdown(context.get());
	context.reset();

	CoUninitialize();
	return exitCode;
}
