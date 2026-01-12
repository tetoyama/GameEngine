#include "buildSetting.h"
#include "mainWindow.h"
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#include "Backends/ImGui/imgui_impl_win32.h"

#include "windowSystem.h"

#include "DebugTools/ImGuiSystem.h"
#include "Graphics/mainRenderer.h"
#include "Platform/InputSystem/InputSystem.h"
#include <Icon/icon.h>
#include "Config/appConfig.h"


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

MainWindow::MainWindow()
	: m_HWND(nullptr), m_ShouldClose(false){}

MainWindow::~MainWindow(){
	if(m_HWND){
		DestroyWindow(m_HWND);
	}
	m_HWND = nullptr;
	UninitIcon();
}

bool MainWindow::Create(const HINSTANCE hInstance, const int nCmdShow, const APPCONFIG appconfig){

	m_width = appconfig.Width;
	m_height = appconfig.Height;

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
	RECT rc = {0,0,m_width ,m_height};

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

	// アイコンの設定
	if(FAILED(InitIcon(m_HWND))){
		OutputDebugStringA("アイコンの初期化に失敗しました。\n");
		return false;
	}
	PollEvents();

	if(appconfig.FullScreen){
		SetBorderlessFullscreen(true);
	}

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

void MainWindow::SetBorderlessFullscreen(bool enable){
	if(!m_HWND) return;

	if(enable && !m_fullscreen){
		m_wpPrev.length = sizeof(WINDOWPLACEMENT);
		GetWindowPlacement(m_HWND, &m_wpPrev);

		MONITORINFO mi = {sizeof(mi)};
		if(GetMonitorInfo(MonitorFromWindow(m_HWND, MONITOR_DEFAULTTONEAREST), &mi)){
			m_width = mi.rcMonitor.right - mi.rcMonitor.left;
			m_height = mi.rcMonitor.bottom - mi.rcMonitor.top;

			// ボーダーレス化
			SetWindowLong(m_HWND, GWL_STYLE, WS_POPUP | WS_VISIBLE);
			SetWindowPos(m_HWND, HWND_TOP,
						 mi.rcMonitor.left, mi.rcMonitor.top,
						 m_width, m_height,
						 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

			// 前面に出す
			ShowWindow(m_HWND, SW_SHOW);
			SetForegroundWindow(m_HWND);

			// レンダラーのリサイズ
			if(m_mainRenderer){
				m_mainRenderer->OnResize(m_width, m_height);
			}

			// 必要ならDirectWrite等の再初期化
			// if (m_mainRenderer && m_mainRenderer->GetGraphicsContext()) {
			//     // 再初期化処理
			// }

			m_fullscreen = true;
			SetTimer(m_HWND, 1, 16, NULL); // 16msごと（約60fps）
		}
	} else if(!enable && m_fullscreen){
		// 復帰
		SetWindowLong(m_HWND, GWL_STYLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

		// クライアントサイズ調整
		m_width = 1280;  // 必要に応じて定数化
		m_height = 720;
		RECT rc = {0, 0, m_width, m_height};
		AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, FALSE);
		int winWidth = rc.right - rc.left;
		int winHeight = rc.bottom - rc.top;

		// 位置とサイズを固定
		SetWindowPos(m_HWND, NULL, m_wpPrev.rcNormalPosition.left, m_wpPrev.rcNormalPosition.top,
					 winWidth, winHeight, SWP_NOZORDER | SWP_FRAMECHANGED);

		ShowWindow(m_HWND, SW_SHOW);
		SetForegroundWindow(m_HWND);

		// レンダラーのリサイズ
		if(m_mainRenderer){
			m_mainRenderer->OnResize(m_width, m_height);
		}

		m_fullscreen = false;
		KillTimer(m_HWND, 1);
	}
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
		case WM_SETICON:
			OutputDebugStringA(("SetIcon wParam:" + std::to_string(wParam) + " lParam:" + std::to_string(wParam) + "\n").c_str());
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_ENTERSIZEMOVE:
			m_resizing = true;
			break;

		case WM_EXITSIZEMOVE:
			m_resizing = false;
			if (m_mainRenderer) {
				m_mainRenderer->OnResize(m_pendingWidth, m_pendingHeight);
				if (m_imguiSystem) {
					m_imguiSystem->OnResize();
				}
			}
			break;

		case WM_SIZE:
			if(LOWORD(lParam) != 0 && HIWORD(lParam) != 0){
				m_pendingWidth = LOWORD(lParam);
				m_pendingHeight = HIWORD(lParam);
			}

			if(m_resizing){

				break;

			} else if (m_mainRenderer) {

				m_mainRenderer->OnResize(m_pendingWidth, m_pendingHeight);
				if (m_imguiSystem) {
					m_imguiSystem->OnResize();
				}
			}
			break;

		case WM_KEYDOWN:
			if(wParam == VK_F11){
				SetBorderlessFullscreen(!m_fullscreen);
			}
			if(wParam == VK_ESCAPE){//エスケープキーが押された
				SendMessage(hwnd, WM_CLOSE, 0, 0);
			}
			break;
		case WM_CLOSE://ウィンドウを閉じたい
			#ifdef _DEBUG_BUILD
				DestroyWindow(hwnd);//終了する手続きをリクエスト
				break;
			#endif
				if(IDOK == MessageBox(hwnd, L"本当に終了してよろしいですか？", L"確認", MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONQUESTION)){	//OKが押された
					DestroyWindow(hwnd);//終了する手続きをリクエスト

				} else{//キャンセル
					return 0;
				}
			break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
