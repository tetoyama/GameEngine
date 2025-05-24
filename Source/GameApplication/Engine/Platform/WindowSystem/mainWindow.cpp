#include "mainWindow.h"
#include <windows.h>

#include "Backends/ImGui/imgui_impl_win32.h"

#include "windowSystem.h"

#include "Engine/DebugTools/ImGuiSystem.h"
#include "Engine/Graphics/mainRenderer.h"
#include "Engine/Platform/InputSystem/InputSystem.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

MainWindow::MainWindow()
	: m_HWND(nullptr), m_ShouldClose(false){}

MainWindow::~MainWindow(){
	if(m_HWND){
		DestroyWindow(m_HWND);
	}
	m_HWND = nullptr;
}

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow){
	// ウィンドウクラス名
	const wchar_t CLASS_NAME[] = L"GameWindowClass";

	// ウィンドウクラス構造体の初期化
	WNDCLASS wc = {};
	wc.lpfnWndProc = StaticWindowProc; // 静的WindowProcを指定
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;

	// ウィンドウクラスの登録
	RegisterClass(&wc);

	//ウィンドウサイズの調整
	RECT rc = {0,0,DEFAULT_WINDOW_WIDTH ,DEFAULT_WINDOW_HEIGHT};

	DWORD DwStyle = WS_OVERLAPPEDWINDOW;

	//描画領域が1280*720になるようにサイズを調整する
	AdjustWindowRect(&rc, DwStyle, FALSE);

	// ウィンドウの生成
	m_HWND = CreateWindowEx(
		0,                  // 拡張スタイル
		CLASS_NAME,         // クラス名
		L"Game Engine",     // ウィンドウタイトル
		DwStyle,			// ウィンドウスタイル
		0, 0,				// 位置
		rc.right - rc.left,		//ウィンドウの横幅
		rc.bottom - rc.top,		//ウィンドウの高さ
		nullptr,            // 親ウィンドウ
		nullptr,            // メニューハンドル
		hInstance,          // インスタンスハンドル
		this                // lpParam: thisポインタを渡す（WM_NCCREATEで受け取る）
	);

	// ウィンドウ生成失敗時
	if(!m_HWND){
		OutputDebugStringA("ウィンドウの生成に失敗しました。\n");
		return false;
	}

	// ウィンドウの表示
	ShowWindow(m_HWND, nCmdShow);
	UpdateWindow(m_HWND);

	return true;
}

void MainWindow::PollEvents(){
	MSG msg = {};
	while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)){
		if(msg.message == WM_QUIT){
			m_ShouldClose = true;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

HWND MainWindow::GetHWND() const{
	return m_HWND;
}

bool MainWindow::ShouldClose() const{
	return m_ShouldClose;
}

LRESULT CALLBACK MainWindow::StaticWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

	ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam);

	MainWindow* pThis = nullptr;
	if(uMsg == WM_NCCREATE){
		// lpCreateParamsからthisポインタを取得し、GWLP_USERDATAに保存
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		pThis = static_cast<MainWindow*>(cs->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
	} else{
		// 2回目以降はGWLP_USERDATAから取得
		pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}
	if(pThis){
		return pThis->WindowProc(hwnd, uMsg, wParam, lParam);
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

	if(m_inputSystem){
		m_inputSystem->MessageUpdateInput(hwnd, uMsg, wParam, lParam);
	}

	switch(uMsg){
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_SIZE:
			if(m_mainRenderer){
				UINT width = LOWORD(lParam);
				UINT height = HIWORD(lParam);
				m_mainRenderer->OnResize(width, height);
			} else{
				OutputDebugStringA("MainRendererが設定されていません。\n");
			}
			if(m_imguiSystem){
				m_imguiSystem->OnResize();
			} else{
				OutputDebugStringA("ImGuiSystemがnullptrです。\n");
			}
			break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
