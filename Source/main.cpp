#include <windows.h>
#include "GameApplication/gameApplication.h"

#ifdef _DEBUG
#include <crtdbg.h>
#endif

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_  HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow){

#ifdef _DEBUG
	// メモリリークの監視フラグ
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// Applicationインスタンス作成
	GameApplication app;

	// 実行
	int exitCode = app.Run(hInstance, nCmdShow);

	// 終了
	return exitCode;
}
