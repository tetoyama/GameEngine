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
	void Initialize(EngineContext* context, HINSTANCE hInstance, int nCmdShow);
	void Shutdown(EngineContext* context);
	void Run(EngineContext* context);
};
