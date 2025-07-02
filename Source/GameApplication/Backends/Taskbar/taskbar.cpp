#include "taskbar.h"

#include <windows.h>
#include <shobjidl.h>
#include <iostream>

static HWND g_hwnd = nullptr;
void InitTaskBar(HWND hwnd){
	g_hwnd = hwnd;
}

void SetTaskBarState(TBPFLAG tbpFlags,HWND hwnd) {
	if(!hwnd){
		hwnd = g_hwnd; // g_hwndが初期化されていない場合はデフォルトのウィンドウハンドルを使用
	}
    ITaskbarList3* pTaskbar = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pTaskbar));
    if (SUCCEEDED(hr)) {

        pTaskbar->SetProgressState(hwnd, tbpFlags); // 進行状態解除
        pTaskbar->Release();

    }
}

void SetTaskBarProgress(int Now, int Max, HWND hwnd) {
	if(!hwnd){
		hwnd = g_hwnd; // g_hwndが初期化されていない場合はデフォルトのウィンドウハンドルを使用
	}
    ITaskbarList3* pTaskbar = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pTaskbar));
    if (SUCCEEDED(hr)) {

        pTaskbar->SetProgressValue(hwnd, Now, Max);
        pTaskbar->Release();
    }
}

/*
	// タスクバーの進行状態を設定する例

    //SetTaskBarState(TBPF_NORMAL); // 通常進行表示
    //for (int i = 0; i <= 100; i++) {
    //	SetTaskBarProgress(i, 100); // i: 現在の値, 100: 最大値
    //	Sleep(1); // 擬似的に進行をシミュレート
    //}
    //SetTaskBarState(TBPF_NOPROGRESS); // 進行状態解除

*/