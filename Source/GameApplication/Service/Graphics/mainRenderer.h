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
		if(d2dRenderer){
			delete d2dRenderer;
		}
	}

	void Initialize(GraphicsContext* context, IWindow* mainWindow) {
		graphicsContext= context;
		m_hwnd = mainWindow->GetHWND();
		d2dRenderer= new D2DRenderer(context, m_hwnd);
		width= mainWindow->GetWidth();
		height= mainWindow->GetHeight();
	}

	void Shutdown()override {}

	void BeginFrame();
	void EndFrame(bool vsync = true);

	void DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color);
	
	void OnResize(UINT width, UINT height){
		m_d2dRenderer->OnResizeRelease();
		if(graphicsContext){
			m_graphicsContext->Resize(width, height);
		}
		m_d2dRenderer->OnResizeRecreate();
		width= width;
		height= height;
	}

	GraphicsContext* GetGraphicsContext() const{
		return graphicsContext;
	}
	HWND GetHWND(){
		return m_hwnd;
	}
private:
	HWND m_hwnd{};
	GraphicsContext* graphicsContext= nullptr;
	D2DRenderer* d2dRenderer= nullptr;

	UINT width= 0;
	UINT height= 0;
};
