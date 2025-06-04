#include <windows.h>
#include "GameApplication/gameApplication.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_  HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow){

	// GameAppインスタンス作成
	GameApplication app;

	// 実行
	return app.Run(hInstance, nCmdShow);
}
