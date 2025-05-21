#pragma once
#include <memory>
#include "engineContext.h"
class EngineContext;

class Engine
{
public:
	void Initialize(std::shared_ptr<EngineContext> context);
	void Shutdown(std::shared_ptr<EngineContext> context);

	void Run(std::shared_ptr<EngineContext> context);
};
