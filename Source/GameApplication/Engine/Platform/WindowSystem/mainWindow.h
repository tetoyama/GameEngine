#pragma once
#include "IWindow.h"

class MainRenderer;
class ImGuiService;
class InputService;
class MainWindow: public IWindow
{
public:
	MainWindow();
	~MainWindow();

	bool Create(HINSTANCE hInstance, int nCmdShow) override;
	void PollEvents() override;
	HWND GetHWND() const override;
	bool ShouldClose() const override;

	void SetMainRenderer(MainRenderer* renderer){
		m_mainRenderer = renderer;
	}
	void SetImGuiSystem(ImGuiService* imguiSystem){
		m_imguiSystem = imguiSystem;
	}
	void SetInputSystem(InputService* inputSystem){
		m_inputSystem = inputSystem;
	}
	UINT GetHeight() const override{
		return m_height;
	}
	UINT GetWidth() const override{
		return m_width;
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
	MainRenderer* m_mainRenderer = nullptr;
	ImGuiService* m_imguiSystem = nullptr;
	InputService* m_inputSystem = nullptr;

	WINDOWPLACEMENT m_wpPrev = {};
	bool m_fullscreen = false;
	int m_width = 0;
	int m_height = 0;
};
