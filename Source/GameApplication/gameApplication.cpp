// =======================================================================
//
// gameApplication.cpp
//
// =======================================================================
#include <windows.h>
#include "gameApplication.h"

#include "engine.h"
#include "engineContext.h"

int GameApplication::Run(HINSTANCE hInstance, int nCmdShow){
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if(FAILED(hr)){
		OutputDebugStringA("CoInitializeEx failed\n");
		return -1;
	}

	EngineContextBuilder builder;
	std::unique_ptr<EngineContext> context = builder.Build();
	if(!context){
		CoUninitialize();
		return -1;
	}

	Engine engine;
	engine.Initialize(context.get(), hInstance, nCmdShow);
	engine.Run(context.get());
	engine.Shutdown(context.get());
	context.reset();

	CoUninitialize();
	return 0;
}
