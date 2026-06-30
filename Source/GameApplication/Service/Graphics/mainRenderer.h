// =======================================================================
// 
// mainRenderer.h
// 
// =======================================================================
#pragma once
#include <chrono>
#include <cstdint>
#include <d2d1.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <dwrite.h>

#include "GraphicsContext.h"
#include "GpuPassTimingProfiler.h"
#include "D2DRenderer.h"
#include "Service/IService.h"

#include "Platform/WindowSystem/mainWindow.h"

// メインウィンドウへの描画を管理するレンダラー
class MainRenderer : public IService{
public:
	MainRenderer(){}

	~MainRenderer() {
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

	void Shutdown()override {
		m_gpuPassTimingProfiler.Reset();
	}

	void BeginFrame();
	void EndFrame(bool vsync = true);

	void DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color);
	
	void OnResize(UINT width, UINT height){
		// WM_SIZEは同じサイズで複数回届くことがある。
		// SwapChain/D2Dリソースの再生成を不要に繰り返さない。
		if(width == 0 || height == 0 || (width == m_width && height == m_height)){
			return;
		}

		const auto begin = std::chrono::steady_clock::now();
		m_d2dRenderer->OnResizeRelease();
		if(m_graphicsContext){
			m_graphicsContext->Resize(width, height);
		}
		m_d2dRenderer->OnResizeRecreate();
		m_width = width;
		m_height = height;
		m_lastResizeCpuTimeSeconds = std::chrono::duration<double>(
			std::chrono::steady_clock::now() - begin
		).count();
		++m_resizeSerial;
	}

	GraphicsContext* GetGraphicsContext() const{
		return m_graphicsContext;
	}

	GpuPassTimingProfiler& GetGpuPassTimingProfiler() noexcept {
		return m_gpuPassTimingProfiler;
	}

	const GpuPassTimingProfiler& GetGpuPassTimingProfiler() const noexcept {
		return m_gpuPassTimingProfiler;
	}

	HWND GetHWND(){
		return m_hwnd;
	}

	double GetLastResizeCpuTimeSeconds() const noexcept {
		return m_lastResizeCpuTimeSeconds;
	}

	uint64_t GetResizeSerial() const noexcept {
		return m_resizeSerial;
	}

private:
	HWND m_hwnd{};
	GraphicsContext* m_graphicsContext = nullptr;
	D2DRenderer* m_d2dRenderer = nullptr;
	GpuPassTimingProfiler m_gpuPassTimingProfiler;

	UINT m_width = 0;
	UINT m_height = 0;
	double m_lastResizeCpuTimeSeconds = 0.0;
	uint64_t m_resizeSerial = 0;
};
