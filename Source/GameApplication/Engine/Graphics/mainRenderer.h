// MainRenderer.h
#pragma once
#include <d2d1.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <dwrite.h>

#include "GraphicsContext.h"
#include "D2DRenderer.h"
#include "Service/IService.h"

#include "Engine/Platform/WindowSystem/mainWindow.h"

#include "Engine/Scene/Component/meshRendererComponent.h"
#include "Engine/Scene/Component/transformComponent.h"
struct RenderViewInfo { /* ƒJƒƒ‰“™‚Ìî•ñ */
};
struct RenderableMesh { /* ƒƒbƒVƒ…î•ñ */
};

class MainRenderer : public IService{
public:
	MainRenderer(){}

	~MainRenderer() {
		m_d2dRenderTarget.Reset();
		m_fontBrush.Reset();
		if(m_d2dRenderer){
			delete m_d2dRenderer;
		}
	}

	void Initialize(GraphicsContext* context, IWindow* mainWindow) {
		m_graphicsContext = context;
		m_hwnd = mainWindow->GetHWND();
		m_d2dRenderer = new D2DRenderer(context, m_hwnd);
		m_width = mainWindow->GetWidth();
		m_height = mainWindow->GetHeight();
	}

	void Shutdown()override {}

	void BeginFrame();
	void EndFrame(bool vsync = true);

	void DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color);
	
	void OnResize(UINT width, UINT height){
		m_d2dRenderer->OnResizeRelease();
		if(m_graphicsContext){
			m_graphicsContext->Resize(width, height);
		}
		m_d2dRenderer->OnResizeRecreate();
		m_width = width;
		m_height = height;
	}

	GraphicsContext* GetGraphicsContext() const{
		return m_graphicsContext;
	}
	HWND GetHWND(){
		return m_hwnd;
	}
private:
	HWND m_hwnd{};
	GraphicsContext* m_graphicsContext = nullptr;
	D2DRenderer* m_d2dRenderer = nullptr;

	Microsoft::WRL::ComPtr<ID2D1RenderTarget> m_d2dRenderTarget;
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_fontBrush;

	UINT m_width = 0;
	UINT m_height = 0;
};
