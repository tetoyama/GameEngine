// =======================================================================
// 
// gameApplication.h
// 
// =======================================================================
#pragma once
#ifndef GAMEENGINE_PORTABLE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif

// アプリケーション全体を管理するクラス
class GameApplication
{
public:
	GameApplication() = default;
	~GameApplication() = default;

	// アプリケーションを起動してエンジンの初期化・実行・終了までを一括で行う
#ifdef GAMEENGINE_PORTABLE
    int Run(int argc, char** argv);
#else
	int Run(HINSTANCE hInstance, int nCmdShow);
#endif
};
