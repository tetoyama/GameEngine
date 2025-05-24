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
struct RenderViewInfo { /* カメラ等の情報 */
};
struct RenderableMesh { /* メッシュ情報 */
};

class MainRenderer : public IService{
public:
	MainRenderer(){}

	~MainRenderer() {
		delete m_d2dRenderer;
	}

	void Initialize(GraphicsContext* context, IWindow* mainWindow) {
		m_graphicsContext = context; m_hwnd = mainWindow->GetHWND(); m_d2dRenderer = new D2DRenderer(context, m_hwnd);
	}

	void BeginFrame();
	void Render(const RenderViewInfo& view, const std::vector<RenderableMesh>& meshes);
	void EndFrame(bool vsync = true);

	void DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color);
	
	void OnResize(UINT width, UINT height){
		m_d2dRenderer->OnResizeRelease();
		if(m_graphicsContext){
			m_graphicsContext->Resize(width, height);
		}
		m_d2dRenderer->OnResizeRecreate();
	}

	void DrawMesh(const MeshRendererComponent* meshRenderer, const TransformComponent* transform){
		if(!meshRenderer || !meshRenderer->mesh) return;

		auto* context = m_graphicsContext->GetContext();

		// 頂点バッファ・インデックスバッファ設定
		UINT stride = sizeof(float) * 7; // 例: position(3) + color(4)
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, meshRenderer->mesh->vertexBuffer.GetAddressOf(), &stride, &offset);
		context->IASetIndexBuffer(meshRenderer->mesh->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// シェーダや定数バッファのセット（仮実装なら省略可）

		// 描画
		context->DrawIndexed(meshRenderer->mesh->indexCount, 0, 0);
	}

	GraphicsContext* GetGraphicsContext() const{
		return m_graphicsContext;
	}
private:
	HWND m_hwnd;
	GraphicsContext* m_graphicsContext = nullptr;
	D2DRenderer* m_d2dRenderer = nullptr;

	Microsoft::WRL::ComPtr<ID2D1RenderTarget> m_d2dRenderTarget;
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_fontBrush;


};
