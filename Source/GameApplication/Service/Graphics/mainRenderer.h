// =======================================================================
// 
// mainRenderer.h
// 
// =======================================================================
#pragma once
#include <chrono>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>

#include "GraphicsContext.h"
#include "GpuPassTimingProfiler.h"
#include "D2DRenderer.h"
#include "Service/IService.h"

#include "Platform/WindowSystem/mainWindow.h"

// メインウィンドウへの描画を管理するレンダラー
class MainRenderer : public IService{
public:
	MainRenderer() = default;
	~MainRenderer() override = default;

	void Initialize(GraphicsContext* context, IWindow* mainWindow) {
		m_graphicsContext = context;
		m_hwnd = mainWindow ? mainWindow->GetHWND() : nullptr;
		m_d2dRenderer = (context && mainWindow)
			? std::make_unique<D2DRenderer>(context, m_hwnd)
			: nullptr;
		m_width = mainWindow ? mainWindow->GetWidth() : 0;
		m_height = mainWindow ? mainWindow->GetHeight() : 0;
	}

	void Shutdown()override {
		m_gpuPassTimingProfiler.Reset();
		// GraphicsContextより先にD2DのDevice依存Resourceを破棄する。
		m_d2dRenderer.reset();
		m_graphicsContext = nullptr;
		m_hwnd = nullptr;
	}

	void BeginFrame();
	void EndFrame(bool vsync = true);

	void DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color);
	void FillRect2D(float x, float y, float width, float height, D2D1::ColorF color);
	
	void OnResize(UINT width, UINT height){
		// WM_SIZEは同じサイズで複数回届くことがある。
		// SwapChain/D2Dリソースの再生成を不要に繰り返さない。
		if(width == 0 || height == 0 || (width == m_width && height == m_height)){
			return;
		}
		if(!m_graphicsContext || !m_d2dRenderer){
			return;
		}

		const auto begin = std::chrono::steady_clock::now();

		// Step 19-A.1 / H2:
		// Resize前のTimestamp Queryは旧Render Target構成に属するため破棄する。
		// ResetはGetData待機を行わず、Query COM ObjectとPending Ringを即時解放する。
		m_gpuPassTimingProfiler.Reset();

		m_d2dRenderer->OnResizeRelease();
		m_graphicsContext->Resize(width, height);

		// ResizeBuffersでDevice Lostを検出した場合、失われたDeviceからD2D資源を
		// 再生成せず、同じPollEventsループ内でWM_QUITを処理させる。
		// これによりEngine::Runは次の描画へ入る前に終了できる。
		if(m_graphicsContext->IsDeviceLost()){
			PostQuitMessage(-1);
			return;
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

	HWND GetHWND() const noexcept {
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
	std::unique_ptr<D2DRenderer> m_d2dRenderer;
	GpuPassTimingProfiler m_gpuPassTimingProfiler;

	UINT m_width = 0;
	UINT m_height = 0;
	double m_lastResizeCpuTimeSeconds = 0.0;
	uint64_t m_resizeSerial = 0;
};