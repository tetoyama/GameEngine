#include "gameApplication.h"
#include <windows.h>

#include "Engine/engine.h"
#include "Engine/engineContext.h"

int GameApplication::Run(HINSTANCE hInstance, int nCmdShow){

	EngineContextBuilder builder;
	std::shared_ptr<EngineContext> context = builder.Build();

	if(!context){
		return -1;
	}

	Engine engine;

	//エンジンの初期化
	engine.Initialize(context, hInstance, nCmdShow);

	//メインループ
	engine.Run(context);

	//エンジンの終了
	engine.Shutdown(context);

	return 0;
}
