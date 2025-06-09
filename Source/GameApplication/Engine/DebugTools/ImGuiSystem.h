#pragma once
#include <memory>
#include "Service/IService.h"

class GraphicsContext;
class IWindow;

class ImGuiService : public IService
{
public:
	bool Initialize(IWindow* window, GraphicsContext* graphics);
	void Shutdown()override;

	void Begin();
	void End();
	void OnResize();
private:
	bool initialized_ = false;
};
