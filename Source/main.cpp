// =======================================================================
// 
// main.cpp
// 
// =======================================================================

// -----------------------------------------------------------------------
// インクルード
// -----------------------------------------------------------------------
#include <windows.h>
#include "gameApplication.h"

//デバッグ用
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC  
#include <cstdlib>  
#include <crtdbg.h>
#endif

// -----------------------------------------------------------------------
// Windowsアプリケーションのエントリーポイント
// -----------------------------------------------------------------------
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow){

#ifdef _DEBUG
	// メモリリークチェックを有効にする（アプリ終了時にリークを報告）
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// アプリケーションインスタンスの生成
	GameApplication app;

	// アプリケーションの実行（初期化 → メインループ → 終了処理）
	int exitCode = app.Run(hInstance, nCmdShow);

	// アプリケーションの終了コードを返す
	return exitCode;
}
