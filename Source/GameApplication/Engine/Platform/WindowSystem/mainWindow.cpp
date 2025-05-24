#include "mainWindow.h"
#include <windows.h>
#include <dwmapi.h> // 必要なら追加
#pragma comment(lib, "dwmapi.lib")
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
		case WM_KEYDOWN:
			if(wParam == VK_F11){
				SetBorderlessFullscreen(!m_fullscreen);
			}
			break;
		//case WM_SYSCOMMAND:
		//	if((wParam & 0xFFF0) == SC_MAXIMIZE){
		//		SetBorderlessFullscreen(!m_fullscreen);
		//		return 0; // デフォルトの最大化処理を抑制
		//	}
		//	if((wParam & 0xFFF0) == SC_RESTORE){
		//		SetBorderlessFullscreen(!m_fullscreen);
		//		return 0; // デフォルトの復元処理を抑制
		//	}
		//	break;
		//case WM_TIMER:
		//	if(m_fullscreen){
		//		POINT pt;
		//		GetCursorPos(&pt);
		//		RECT winRect;
		//		GetWindowRect(m_HWND, &winRect);
		//		// 上端2px以内か判定
		//		bool onTop = (pt.x >= winRect.left && pt.x < winRect.right && pt.y >= winRect.top && pt.y <= winRect.top + 25);

		//		static bool captionShown = false;
		//		if(onTop && !captionShown){
		//			// 1. 新しいスタイル
		//			LONG style = GetWindowLong(m_HWND, GWL_STYLE);
		//			style |= (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME);

		//			// 2. クライアント領域をモニター全体に合わせるためのウィンドウサイズを計算
		//			MONITORINFO mi = {sizeof(mi)};
		//			GetMonitorInfo(MonitorFromWindow(m_HWND, MONITOR_DEFAULTTONEAREST), &mi);
		//			RECT rc = mi.rcMonitor; // クライアント領域をモニター全体にしたい

		//			RECT winRect = rc;
		//			AdjustWindowRect(&winRect, style, FALSE);
		//			int winWidth = rc.right - rc.left;
		//			int winHeight = winRect.bottom - winRect.top;

		//			// 3. スタイルを適用
		//			SetWindowLong(m_HWND, GWL_STYLE, style);

		//			// 4. ウィンドウの位置・サイズを再設定
		//			SetWindowPos(m_HWND, HWND_TOP,
		//						0, // 枠線分だけ左上をずらす
		//						0,
		//						 winWidth, winHeight,
		//						 SWP_NOZORDER | SWP_FRAMECHANGED);

		//			captionShown = true;
		//		} else if(!onTop && captionShown){
		//			// 枠線・キャプションを外す
		//			LONG style = GetWindowLong(m_HWND, GWL_STYLE);
		//			style &= ~(WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME);

		//			// クライアント領域をモニター全体に戻す
		//			MONITORINFO mi = {sizeof(mi)};
		//			GetMonitorInfo(MonitorFromWindow(m_HWND, MONITOR_DEFAULTTONEAREST), &mi);
		//			RECT rc = mi.rcMonitor;

		//			// WS_POPUP時はAdjustWindowRect不要
		//			SetWindowLong(m_HWND, GWL_STYLE, style);
		//			SetWindowPos(m_HWND, HWND_TOP,
		//						 rc.left, rc.top,
		//						 rc.right - rc.left, rc.bottom - rc.top,
		//						 SWP_NOZORDER | SWP_FRAMECHANGED);

		//			captionShown = false;
		//		}
		//	}
		//	break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
