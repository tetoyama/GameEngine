#pragma once
#include <windows.h>

// アプリケーション全体を管理するクラス
class GameApplication
{
public:
	GameApplication(){}
	~GameApplication(){}

	// アプリケーションの実行（エンジン起動、ループ処理など）
	int Run(HINSTANCE hInstance, int nCmdShow);
};
