#pragma once
#include <memory>

class GraphicsContext;
class IWindow;

class ImGuiSystem
{
public:
	bool Initialize(IWindow* window, GraphicsContext* graphics);
	void Begin();
	void End();
	void OnResize();
private:
	bool initialized_ = false;
};
