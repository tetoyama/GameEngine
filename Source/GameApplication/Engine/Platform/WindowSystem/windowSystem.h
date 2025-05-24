#pragma once
#include <memory>
#include "Service/IService.h"
#include "IWindow.h"
#include "mainWindow.h"

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

class WindowSystem : public IService
{
public:
	bool Initialize(HINSTANCE hInstance, int nCmdShow);
	void PollEvents();
	std::shared_ptr<MainWindow> GetMainWindow() const;

private:
	std::shared_ptr<MainWindow> m_MainWindow;
};
