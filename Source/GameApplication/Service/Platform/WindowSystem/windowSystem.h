// =======================================================================
// 
// windowSystem.h
// 
// =======================================================================
#pragma once
#include <memory>
#include "Service/IService.h"
#include "IWindow.h"
#include "mainWindow.h"

class DebugLogService;

class WindowService : public IService
{
public:
	explicit WindowService(DebugLogService* debugLog = nullptr)
		: m_DebugLog(debugLog)
	{}

	bool Initialize(const HINSTANCE hInstance, const int nCmdShow, const APPCONFIG appconfig);
	void Shutdown()override {}

	void PollEvents();
	std::shared_ptr<MainWindow> GetMainWindow() const;

private:
	std::shared_ptr<MainWindow> m_MainWindow;
	DebugLogService* m_DebugLog = nullptr;
};
