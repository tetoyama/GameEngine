#pragma once
#include <memory>
#include "Service/IService.h"
#include <DirectXMath.h>

class GraphicsContext;
class IWindow;
class ImGuiManubar;

class ImGuiService : public IService
{
public:
	bool Initialize(IWindow* window, GraphicsContext* graphics);
	void Shutdown()override;

	void SetViewProjectionMatrix(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);
	DirectX::XMMATRIX RenderGizmo(const DirectX::XMMATRIX& world) const;

	void Begin();
	void End();
	void OnResize();

	std::shared_ptr<ImGuiManubar> GetManubar() const{
		return manubar;
	}

private:
	bool initialized_ = false;
	GraphicsContext* m_GraphicsContext = nullptr;
	DirectX::XMMATRIX m_view;
	DirectX::XMMATRIX m_projection;

	std::shared_ptr<ImGuiManubar> manubar;
};
