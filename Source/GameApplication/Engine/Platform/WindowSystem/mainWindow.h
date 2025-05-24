#pragma once
#include "IWindow.h"

class MainRenderer;
class ImGuiSystem;
class InputSystem;
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
	void SetImGuiSystem(ImGuiSystem* imguiSystem){
		m_imguiSystem = imguiSystem;
	}
	void SetInputSystem(InputSystem* inputSystem){
		m_inputSystem = inputSystem;
	}
	UINT GetHeight() const override{
		return m_height;
	}
	UINT GetWidth() const override{
		return m_width;
	}
	void SetBorderlessFullscreen(bool enable);
private:
	static LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


	HWND m_HWND;
	bool m_ShouldClose;
	MainRenderer* m_mainRenderer = nullptr;
	ImGuiSystem* m_imguiSystem = nullptr;
	InputSystem* m_inputSystem = nullptr;

	WINDOWPLACEMENT m_wpPrev = {};
	bool m_fullscreen = false;
	int m_width = 0;
	int m_height = 0;
};
