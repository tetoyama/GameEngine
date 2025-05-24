#pragma once
#include <memory>
#include "Service/IService.h"

class GraphicsContext;
class IWindow;

class ImGuiSystem : public IService
{
public:
	bool Initialize(IWindow* window, GraphicsContext* graphics);
	void Begin();
	void End();
	void OnResize();
private:
	bool initialized_ = false;
};
