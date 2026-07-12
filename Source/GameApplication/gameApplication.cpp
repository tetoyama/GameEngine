// =======================================================================
// 
// gameApplication.cpp
// 
// =======================================================================
#include <windows.h>
#include "gameApplication.h"

#include "engine.h"
#include "engineContext.h"
#include "Game/MiniGameCollection/Runtime/MiniGameCollectionBootstrap.h"

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
		if(!scenes ||
		   !MiniGameCollection::Runtime::MiniGameCollectionBootstrap::Load(
			   *scenes.get())){
			exitCode = -1;
			OutputDebugStringA(
				"MiniGameCollectionBootstrap failed to load the persistent scene\n"
			);
		}else{
			engine.Run(context.get());
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
