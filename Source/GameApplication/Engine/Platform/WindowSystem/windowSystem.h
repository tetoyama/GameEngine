#pragma once
#include <memory>
#include "IWindow.h"

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

class WindowSystem
{
public:
	bool Initialize(HINSTANCE hInstance, int nCmdShow);
	void PollEvents();
	std::shared_ptr<IWindow> GetMainWindow() const;

private:
	std::shared_ptr<IWindow> m_MainWindow;
};
