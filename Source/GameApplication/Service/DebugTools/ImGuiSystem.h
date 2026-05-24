// =======================================================================
// 
// ImGuiSystem.h
// 
// =======================================================================
#pragma once
#include <memory>
#include <DirectXMath.h>
#include <Windows.h>

#include "Service/IService.h"

class GraphicsContext;
class IWindow;

// ImGuiの初期化・終了・描画補助を管理するサービス
class ImGuiService : public IService
{
public:
	bool Initialize(IWindow* window, GraphicsContext* graphics);
	void Shutdown()override;

	void SetViewProjectionMatrix(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);
	DirectX::XMMATRIX RenderGizmo(const DirectX::XMMATRIX& world) const;
	DirectX::XMMATRIX RenderGizmo2D(const DirectX::XMMATRIX& world, const DirectX::XMFLOAT2& vp) const;

	void Begin();
	void End();
	void OnResize();

private:
	bool m_Initialized= false;
	GraphicsContext* m_GraphicsContext = nullptr;
	DirectX::XMMATRIX m_View;
	DirectX::XMMATRIX m_Projection;
};
