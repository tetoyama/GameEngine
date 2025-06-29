// Engine/Scene/System/renderSystem.h
#pragma once
#include "Interface/ISystem.h"
#include <d3d11.h>
#include <wrl/client.h> 

struct SceneContext;

class TransformComponent;
class TextureComponent;
class MeshRendererComponent;
class ModelRendererComponent;
class BillBoardRendererComponent;

class RenderSystem : public ISystem{
public:
	RenderSystem(SceneContext* context): m_context(context){}
	~RenderSystem(){}

	void Initialize() override;
	void Finalize()override;

	void Start() override {};
	void Update(float deltaTime) override{};
	void FixedUpdate(float fidedDeltaTime) override {}
	void Draw() override;
	void EditorUpdate(float deltaTime) override{}

private:
	void SetRenderTarget(ID3D11DeviceContext* ctx, ID3D11DepthStencilView* dsv = nullptr){
		D3D11_VIEWPORT viewport = {
		0.0f,              // TopLeftX
		0.0f,              // TopLeftY
		1280.0f,           // Width
		720.0f,            // Height
		0.0f,              // MinDepth
		1.0f               // MaxDepth
		};
		ctx->OMSetRenderTargets(1, &rtv, dsv);
		ctx->RSSetViewports(1, &viewport);
	}

	void Clear(ID3D11DeviceContext* ctx, const float clear[4]){
		ctx->ClearRenderTargetView(rtv, clear);
	}
	ID3D11ShaderResourceView* GetSRV() const{
		return srv;
	}

	void DrawMesh(TransformComponent* pTransform, MeshRendererComponent* pMesh, TextureComponent* pTexture);
	void DrawModel(TransformComponent* pTransform, ModelRendererComponent* pMesh, TextureComponent* pTexture);
	void DrawBillBoard(TransformComponent* pTransform, MeshRendererComponent* pMesh, BillBoardRendererComponent* pBillBoard, TextureComponent* pTexture);

	void DrawEntities();

	void SetCameraView();

	void EditorView();
	void PlayerView();

	SceneContext* m_context;
	MeshRendererComponent* m_billBoardMesh = nullptr;
	ID3D11Texture2D* tex = nullptr;
	ID3D11RenderTargetView* rtv = nullptr;
	ID3D11ShaderResourceView* srv = nullptr;
	ID3D11DepthStencilView* dsv = nullptr;

};
