// =======================================================================
// 
// mainRenderer.h
// 
// =======================================================================
#pragma once
#include <d2d1.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <dwrite.h>

#include "GraphicsContext.h"
#include "D2DRenderer.h"
#include "Service/IService.h"

#include "Platform/WindowSystem/mainWindow.h"

// メインウィンドウへの描画を管理するレンダラー
class MainRenderer : public IService{
public:
	MainRenderer(){}

	~MainRenderer() {
		if(m_pD2dRenderer){
			delete d2dRenderer;
		}
	}

	void Initialize(GraphicsContext* context, IWindow* mainWindow) {
		m_pGraphicsContext = context;
		m_hwnd = mainWindow->GetHWND();
		m_pD2dRenderer = new D2DRenderer(context, m_hwnd);
		m_width = mainWindow->GetWidth();
		m_height = mainWindow->GetHeight();
	}

	void Shutdown()override {}

	void BeginFrame();
	void EndFrame(bool vsync = true);

	void DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color);
	
	void OnResize(UINT width, UINT height){
		m_pD2dRenderer->OnResizeRelease();
		if(m_pGraphicsContext){
			m_pGraphicsContext->Resize(width, height);
		}
		m_pD2dRenderer->OnResizeRecreate();
		m_width = width;
		m_height = height;
	}

	GraphicsContext* GetGraphicsContext() const{
		return graphicsContext;
	}
	HWND GetHWND(){
		return hwnd;
	}
private:
	HWND m_Hwnd{};
	GraphicsContext* m_pGraphicsContext = nullptr;
	D2DRenderer* m_pD2dRenderer = nullptr;

	UINT m_Width= 0;
	UINT m_Height= 0;
};
