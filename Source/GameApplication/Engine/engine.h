// =======================================================================
//
// engine.h
//
// =======================================================================
#pragma once

#include <windows.h>

class EngineContext;

class Engine {
public:
	bool Initialize(EngineContext* context, HINSTANCE hInstance, int nCmdShow);
	void Shutdown(EngineContext* context);
	void Run(EngineContext* context);
private:
	bool m_sdlVideoInitialized = false;
};
