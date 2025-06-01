#include "MainRenderer.h"
#include <dwrite.h>
#include <d2d1.h>
#include <vector>

void MainRenderer::DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color){
	m_d2dRenderer->DrawText2D(text, x, y, fontSize, color);
}

void MainRenderer::BeginFrame() {

    float clearColor[4] = {0.5f, 0.5f, 0.5f, 1.0f};
	m_graphicsContext->Clear(clearColor);
}

void MainRenderer::EndFrame(bool vsync) {
	m_graphicsContext->Present(vsync);
}
