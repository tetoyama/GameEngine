#include "MainRenderer.h"
#include <dwrite.h>
#include <d2d1.h>
#include <vector>

void MainRenderer::DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color){
	m_d2dRenderer->DrawText2D(text, x, y, fontSize, color);
}

void MainRenderer::BeginFrame() {

    float clearColor[4] = {0.1f, 0.1f, 0.1f, 1.0f};
	m_graphicsContext->Clear(clearColor);

    // デプスステンシルステートやブレンドステートのセットアップ（必要に応じて）
    auto context = m_graphicsContext->GetContext();
    auto depthStencilView = m_graphicsContext->GetDepthStencilView();

    if (context && depthStencilView) {
        // デプスバッファもクリア
        context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

		// レンダーターゲットビューをセット
        ID3D11RenderTargetView* renderTargetView = m_graphicsContext->GetRenderTargetView();
        context->OMSetRenderTargets(1, &renderTargetView, depthStencilView);

        // 必要に応じてデプス/ブレンド/ラスタライザステートをセット
        // 例: context->OMSetDepthStencilState(...);
        // 例: context->OMSetBlendState(...);
        // 例: context->RSSetState(...);
    }
}

void MainRenderer::Render(const RenderViewInfo& view, const std::vector<RenderableMesh>& meshes) {


    for (const auto& mesh : meshes) {
        // メッシュ描画処理
    }
}

void MainRenderer::EndFrame(bool vsync) {
	m_graphicsContext->Present(vsync);
}
