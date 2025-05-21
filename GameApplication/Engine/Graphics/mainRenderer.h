// MainRenderer.h
#pragma once
#include <d2d1.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <dwrite.h>

#include "GraphicsContext.h"
#include "D2DRenderer.h"

#include "Engine/Platform/WindowSystem/mainWindow.h"

struct RenderViewInfo { /* ƒJƒƒ‰“™‚Ìî•ñ */
};
struct RenderableMesh { /* ƒƒbƒVƒ…î•ñ */
};

class MainRenderer {
public:
	MainRenderer(GraphicsContext* context, IWindow* mainWindow)
		: m_graphicsContext(context), m_hwnd(mainWindow->GetHWND()), m_d2dRenderer(context, m_hwnd){}

	~MainRenderer();

	void BeginFrame();
	void Render(const RenderViewInfo& view, const std::vector<RenderableMesh>& meshes);
	void EndFrame(bool vsync = true);

	void DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color);
	
	void OnResize(UINT width, UINT height){
		m_d2dRenderer.OnResizeRelease();
		if(m_graphicsContext){
			m_graphicsContext->Resize(width, height);
		}
		m_d2dRenderer.OnResizeRecreate();
	}
private:
	HWND m_hwnd;
	GraphicsContext* m_graphicsContext;
	D2DRenderer m_d2dRenderer;

	Microsoft::WRL::ComPtr<ID2D1RenderTarget> m_d2dRenderTarget;
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_fontBrush;


};
