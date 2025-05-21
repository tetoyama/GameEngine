#include "windowSystem.h"
#include "mainWindow.h"

bool WindowSystem::Initialize(HINSTANCE hInstance, int nCmdShow){
	m_MainWindow = std::make_shared<MainWindow>();
	return m_MainWindow->Create(hInstance, nCmdShow);
}

void WindowSystem::PollEvents(){
	if(m_MainWindow){
		m_MainWindow->PollEvents();
	}
}

std::shared_ptr<IWindow> WindowSystem::GetMainWindow() const{
	return m_MainWindow;
}
