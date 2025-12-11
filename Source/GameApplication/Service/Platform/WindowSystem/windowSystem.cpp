#include "windowSystem.h"
#include "mainWindow.h"

bool WindowService::Initialize(HINSTANCE hInstance, int nCmdShow){
	m_MainWindow = std::make_shared<MainWindow>();
	return m_MainWindow->Create(hInstance, nCmdShow);
}

void WindowService::PollEvents(){
	if(m_MainWindow){
		m_MainWindow->PollEvents();
	}
}

std::shared_ptr<MainWindow> WindowService::GetMainWindow() const{
	return m_MainWindow;
}
