// Engine/Scene/System/renderSystem.h
#pragma once
#include "Interface/ISystem.h"
#include <d3d11.h>
#include <wrl/client.h> 
#include "Backends/myVector2.h"
#include "Backends/myVector3.h"
#include "Component/RenderLayerComponent.h"

struct SceneContext;

class TransformComponent;
class TextureComponent;
class SpriteRendererComponent;
class MeshRendererComponent;
class ModelRendererComponent;
class BillBoardRendererComponent;
class ParticleComponent;

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
	void EditorUpdate(float deltaTime) override;

private:
	void DrawRenderLayerToggleUI();

	TransformComponent CalculateRectTransform(const SpriteRendererComponent& sprite, const TransformComponent& transform);

	void DrawMesh(TransformComponent* pTransform, MeshRendererComponent* pMesh, TextureComponent* pTexture);
	void DrawModel(TransformComponent* pTransform, ModelRendererComponent* pMesh, TextureComponent* pTexture);
	void DrawBillBoard(TransformComponent* pTransform, MeshRendererComponent* pMesh, BillBoardRendererComponent* pBillBoard, TextureComponent* pTexture);
	void DrawParticle(TransformComponent* pTransform, ParticleComponent* pParticle, TextureComponent* pTexture);

	void DrawEntities(bool* RenderLayer);

	void SetCameraView();
	void SetEditorCameraView();

	void ControllButton();

	void EditorView();
	void PlayerView();

	SceneContext* m_context;
	MeshRendererComponent* m_billBoardMesh = nullptr;
	MeshRendererComponent* m_SpriteMesh = nullptr;
	ID3D11Texture2D* tex_player = nullptr;
	ID3D11RenderTargetView* rtv_player = nullptr;
	ID3D11ShaderResourceView* srv_player = nullptr;
	ID3D11DepthStencilView* dsv_player = nullptr;

	ID3D11Texture2D* tex_editor = nullptr;
	ID3D11RenderTargetView* rtv_editor = nullptr;
	ID3D11ShaderResourceView* srv_editor = nullptr;
	ID3D11DepthStencilView* dsv_editor = nullptr;

	Vector2 m_ScreenSize = Vector2(1280.0f, 720.0f);

	Vector3 m_EditorCameraPosition = Vector3(0.0f, 5.0f, -20.0f);
	Vector3 m_EditorCameraRotation = Vector3(0.0f, 0.0f, 0.0f);

	Vector3 m_CameraPosition = Vector3(0.0f, 0.0f, 0.0f);

	DirectX::XMMATRIX m_CameraView = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX m_CameraProjection = DirectX::XMMatrixIdentity();
	float m_MouseWheel = 0.0f;

	bool* showPlayer = nullptr;
	bool* showEditor = nullptr;

	bool editorRenderLayerVisible[(int)RenderLayer::MaxRenderLayer] = {
	true, true, true, true, true, true
	};

	bool playerRenderLayerVisible[(int)RenderLayer::MaxRenderLayer] = {
	true, true, true, true, true, true
	};

	int currentSelectedLayer = (int)RenderLayer::Opaque3D; // 初期選択（例）

};
