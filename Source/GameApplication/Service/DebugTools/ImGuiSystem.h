#pragma once
#include <memory>
#include <DirectXMath.h>
#include <Windows.h>

#include "Service/IService.h"

class GraphicsContext;
class IWindow;

class ImGuiService : public IService
{
public:
	bool Initialize(IWindow* window, GraphicsContext* graphics);
	void Shutdown()override;

	void SetViewProjectionMatrix(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);
	DirectX::XMMATRIX RenderGizmo(const DirectX::XMMATRIX& world) const;
	DirectX::XMMATRIX RenderGizmo2D(const DirectX::XMMATRIX& world) const;

	void Begin();
	void End();
	void OnResize();

private:
	bool initialized_ = false;
	GraphicsContext* m_GraphicsContext = nullptr;
	DirectX::XMMATRIX m_view;
	DirectX::XMMATRIX m_projection;
};
