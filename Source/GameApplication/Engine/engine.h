#pragma once
#include <memory>
#include <windows.h>

class EngineContext;
class Engine
{
public:
	void Initialize(std::shared_ptr<EngineContext> context, HINSTANCE hInstance, int nCmdShow);
	void Shutdown(std::shared_ptr<EngineContext> context);

	void Run(std::shared_ptr<EngineContext> context);
};
