#include "MainRenderer.h"
#include <dwrite.h>
#include <d2d1.h>
#include <vector>

MainRenderer::~MainRenderer(){
	m_d2dRenderTarget.Reset();
	m_fontBrush.Reset();
}

void MainRenderer::DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color){
	m_d2dRenderer.DrawText2D(text, x, y, fontSize, color);
}

void MainRenderer::BeginFrame() {

    float clearColor[4] = {0.1f, 0.1f, 0.1f, 1.0f};
	m_graphicsContext->Clear(clearColor);
}

void MainRenderer::Render(const RenderViewInfo& view, const std::vector<RenderableMesh>& meshes) {


    for (const auto& mesh : meshes) {
        // ƒƒbƒVƒ…•`‰æˆ—
    }
}

void MainRenderer::EndFrame(bool vsync) {
	m_graphicsContext->Present(vsync);
}
