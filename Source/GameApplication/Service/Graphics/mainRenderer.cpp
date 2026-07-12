// =======================================================================
// 
// mainRenderer.cpp
// 
// =======================================================================
#include "MainRenderer.h"
#include <dwrite.h>
#include <d2d1.h>
#include <vector>

void MainRenderer::DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color){
	if(m_d2dRenderer){
		m_d2dRenderer->DrawText2D(text, x, y, fontSize, color);
	}
}

void MainRenderer::FillRect2D(
	float x,
	float y,
	float width,
	float height,
	D2D1::ColorF color
){
	if(m_d2dRenderer){
		m_d2dRenderer->FillRect2D(x, y, width, height, color);
	}
}

void MainRenderer::BeginFrame() {
	if(!m_graphicsContext || m_graphicsContext->IsDeviceLost()){
		return;
	}

	float clearColor[4] = {0.5f, 0.5f, 0.5f, 1.0f};
	m_graphicsContext->Clear(clearColor);
}

void MainRenderer::EndFrame(bool vsync) {
	if(!m_graphicsContext || m_graphicsContext->IsDeviceLost()){
		m_gpuPassTimingProfiler.Reset();
		return;
	}

	m_graphicsContext->Present(vsync);

	// PresentでDevice Lostが確定した時点で、現在Frameを含む未回収Timestamp Queryを
	// GetData待機なしで破棄する。次Frameで失われたDeviceのQueryを参照しない。
	if(m_graphicsContext->IsDeviceLost()){
		m_gpuPassTimingProfiler.Reset();
	}
}