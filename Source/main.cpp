// =======================================================================
// 
// main.cpp
// 
// =======================================================================

// -----------------------------------------------------------------------
// インクルード
// -----------------------------------------------------------------------
#include <windows.h>
#include "buildSetting.h"
#include "gameApplication.h"

//デバッグ用
#ifdef _DEBUG_BUILD
#define _CRTDBG_MAP_ALLOC  
#include <cstdlib>  
#include <crtdbg.h>
#endif
#include <exception>

// -----------------------------------------------------------------------
// Windowsアプリケーションのエントリーポイント
// -----------------------------------------------------------------------
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow) {

    #ifdef _DEBUG_BUILD
    // メモリリークチェックを有効にする（アプリ終了時にリークを報告）
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    #endif

    try {
        // アプリケーションインスタンスの生成
        GameApplication app;

        // アプリケーションの実行（初期化 → メインループ → 終了処理）
        int exitCode = app.Run(hInstance, nCmdShow);

        // アプリケーションの終了コードを返す
        return exitCode;
    }
    catch (const std::exception& e) {
        MessageBoxA(
            nullptr,
            e.what(),
            "Fatal Error",
            MB_OK | MB_ICONERROR);

        return -1;
    }
    catch (...) {
        MessageBoxA(
            nullptr,
            "Unknown Exception",
            "Fatal Error",
            MB_OK | MB_ICONERROR);

        return -1;
    }
}
