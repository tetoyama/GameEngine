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
		delete m_d2dRenderer;
	}

	void Initialize(GraphicsContext* context, IWindow* mainWindow) {
		m_graphicsContext = context; m_hwnd = mainWindow->GetHWND(); m_d2dRenderer = new D2DRenderer(context, m_hwnd);
		m_width = mainWindow->GetWidth();
		m_height = mainWindow->GetHeight();
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
		m_width = width;
		m_height = height;
	}

    void DrawMesh(const MeshRendererComponent* meshRenderer, const TransformComponent* transform) {  
       if (!meshRenderer || !meshRenderer->mesh) return;  

       auto context = m_graphicsContext->GetContext();  

       context->IASetInputLayout(m_graphicsContext->GetVertexLayout().Get());  

       context->VSSetShader(m_graphicsContext->GetVertexShader().Get(), NULL, 0);  
       context->PSSetShader(m_graphicsContext->GetPixelShader().Get(), NULL, 0);  

       m_graphicsContext->SetWorldViewProjection2D();  

       UINT stride = meshRenderer->mesh->GetVertexStride();  
       UINT offset = 0;  

       ID3D11Buffer* vertexBuffer = meshRenderer->mesh->GetVertexBuffer();  
       context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);  

       context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);  

       context->Draw(meshRenderer->mesh->GetIndexCount(), 0);  
    }

	GraphicsContext* GetGraphicsContext() const{
		return m_graphicsContext;
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
