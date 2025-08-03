#include "gameApplication.h"
#include <windows.h>

#include "Engine/engine.h"
#include "Engine/engineContext.h"

// アプリケーションの実行メソッド（初期化 → ループ → 終了）
int GameApplication::Run(HINSTANCE hInstance, int nCmdShow){

	// COMライブラリの初期化
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if(FAILED(hr)){
		OutputDebugStringA("CoInitializeEx failed\n");
		return -1;
	}

	// EngineContextのビルダーを使用して、DIコンテナなどの初期環境を構築
	EngineContextBuilder builder;
	std::shared_ptr<EngineContext> context = builder.Build();

	// コンテキスト生成に失敗した場合は終了
	if(!context){
		return -1;
	}

	// エンジン初期化（ウィンドウ作成、DirectXデバイス準備など）
	Engine engine;
	engine.Initialize(context, hInstance, nCmdShow);

	// メインループ（ゲームロジックや描画処理のループ）
	engine.Run(context);

	// 終了処理（リソース解放、終了ログなど）
	engine.Shutdown(context);

	// COMライブラリの終了
	CoUninitialize();

	return 0;
}