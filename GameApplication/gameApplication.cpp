#include "gameApplication.h"
#include <windows.h>

#include "Engine/engine.h"
#include "Engine/engineContext.h"

GameApplication::GameApplication(){

}
GameApplication::~GameApplication(){

}

int GameApplication::Run(HINSTANCE hInstance, int nCmdShow){

	EngineContextBuilder builder;
	std::shared_ptr<EngineContext> context = builder.Build(hInstance, nCmdShow);
	if(!context){
		// エラー処理（ログ出力など）
		return -1;
	}

	Engine engine;

	//エンジンの初期化
	engine.Initialize(context);

	//メインループ
	engine.Run(context);

	//エンジンの終了
	engine.Shutdown(context);

	return 0;
}
