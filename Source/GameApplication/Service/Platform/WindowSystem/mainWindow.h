// =======================================================================
// 
// mainWindow.h
// 
// =======================================================================
#pragma once
#include "IWindow.h"

struct APPCONFIG;

class mainRenderer;
class imGuiService;
class inputService;
class debugLogService;

class MainWindow: public IWindow
{
public:
	explicit MainWindow(DebugLogService* debugLog = nullptr);
	~MainWindow();

	bool Create(const HINSTANCE hInstance, const int nCmdShow, const APPCONFIG appconfig) override;
	void PollEvents() override;
	HWND GetHWND() const override;
	bool ShouldClose() const override;

	void SetMainRenderer(MainRenderer* renderer){
		m_pMainRenderer = pRenderer;
	}
	void SetImGuiSystem(ImGuiService* imguiSystem){
		m_pImguiSystem = imguiSystem;
	}
	void SetInputSystem(InputService* inputSystem){
		m_pInputSystem = inputSystem;
	}
	UINT GetHeight() const override{
		return height;
	}
	UINT GetWidth() const override{
		return width;
	}
	void SetBorderlessFullscreen(bool enable);

	void Close(){
		PostMessage(m_HWND, WM_CLOSE, 0, 0);
	}
private:
	static LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


	HWND m_HWND;
	bool m_ShouldClose;
	MainRenderer* m_pMainRenderer = nullptr;
	ImGuiService* m_pImguiSystem = nullptr;
	InputService* m_pInputSystem = nullptr;
	DebugLogService* m_pDebugLog = nullptr;

	bool m_Resizing= false;

	UINT m_PendingWidth= 1280;
	UINT m_PendingHeight= 720;

	WINDOWPLACEMENT m_WpPrev= {};
	bool m_Fullscreen= false;
	int m_Width= 0;
	int m_Height= 0;
};
