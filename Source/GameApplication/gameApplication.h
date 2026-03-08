// =======================================================================
// 
// gameApplication.h
// 
// =======================================================================
#pragma once
#include <windows.h>

// アプリケーション全体を管理するクラス
class GameApplication
{
public:
	GameApplication(){}
	~GameApplication(){}

	// アプリケーションを起動してエンジンの初期化・実行・終了までを一括で行う
	int Run(HINSTANCE hInstance, int nCmdShow);
};
