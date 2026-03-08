// =======================================================================
// 
// windowSystem.cpp
// 
// =======================================================================
#include "windowSystem.h"
#include "mainWindow.h"
#include "Config/appConfig.h"

bool WindowService::Initialize(const HINSTANCE hInstance, const int nCmdShow, const APPCONFIG appconfig, DebugLogService* debugLog){
	m_MainWindow = std::make_shared<MainWindow>();
	m_MainWindow->SetDebugLogService(debugLog);
	return m_MainWindow->Create(hInstance, nCmdShow,appconfig);
}

void WindowService::PollEvents(){
	if(m_MainWindow){
		m_MainWindow->PollEvents();
	}
}

std::shared_ptr<MainWindow> WindowService::GetMainWindow() const{
	return m_MainWindow;
}
