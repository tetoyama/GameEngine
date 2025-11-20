// Engine/Scene/System/renderSystem.h
#pragma once
#include "Interface/ISystem.h"
#include <d3d11.h>
#include <wrl/client.h> 
#include "Backends/myVector2.h"
#include "Backends/myVector3.h"
#include "Component/RenderLayerComponent.h"
#include "../Entity/Entity.h"

struct SceneManagerContext;
struct PixelShaderData;
struct VertexShaderData;

class ComponentRegistry;
class TransformComponent;
class TextureComponent;
class SpriteRendererComponent;
class MeshRendererComponent;
class TerrainComponent;
class WaveComponent;
class ModelRendererComponent;
class BillBoardRendererComponent;
class ParticleComponent;
class OutlineComponent;
class CameraComponent;

struct PostEffect {

	PostEffectShader* shader;
	std::string name;
	bool enabled;
};

enum RenderPassType {
	SHADOW_PASS  = 0,
	GBUFFER_PASS,
};

struct CameraEntityData {

	CameraComponent* cameraComponent = nullptr;
	TransformComponent* transformComponent = nullptr;

	// 念のためエンティティも保持
	Entity entity = 0;
	SceneContext* sceneContext = nullptr;
};

struct RenderPassContext {

	RenderPassContext(
		const RenderPassType& renderPass, 
		bool* renderLayer, 
		std::shared_ptr<PixelShaderData> setPixelShader,
		std::shared_ptr<VertexShaderData> setVertexShader, 
		const CameraEntityData& data, 
		const Vector2& setScreenSize
	);

	bool* renderLayerVisibility = nullptr;
	RenderPassType passType = SHADOW_PASS;

	DirectX::XMFLOAT4 cameraPosition = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixIdentity();

	std::shared_ptr<PixelShaderData> pixelShader;
	std::shared_ptr<VertexShaderData> vertexShader;

	CameraEntityData cameraData;
	Vector2 screenSize = Vector2(1280.0f, 720.0f);

};

class RenderSystem : public ISystem{
public:
	RenderSystem(SceneManagerContext* context): m_context(context){}
	~RenderSystem(){}

	void Initialize() override;
	void Finalize()override;

	void Start() override {};
	void Update(float deltaTime) override;
	void FixedUpdate(float fidedDeltaTime) override {}
	void Draw() override;
	void EditorUpdate(float deltaTime) override;

private:


	TransformComponent CalculateRectTransform(const RenderPassContext& renderPassContext, const SpriteRendererComponent& sprite, const TransformComponent& transform);

	const CameraEntityData FindCameraEntity();

	void DrawEntities(const RenderPassContext& renderPassContext);

	void DrawMesh(ComponentRegistry* componentRegistry, TransformComponent* pTransform, MeshRendererComponent* pMesh, TextureComponent* pTexture);
	void DrawModel(ComponentRegistry* componentRegistry, TransformComponent* pTransform, ModelRendererComponent* pMesh, TextureComponent* pTexture, OutlineComponent* pOutline);
	void DrawBillBoard(ComponentRegistry* componentRegistry, TransformComponent* pTransform, MeshRendererComponent* pMesh, BillBoardRendererComponent* pBillBoard, TextureComponent* pTexture);
	void DrawParticle(ComponentRegistry* componentRegistry, TransformComponent* pTransform, ParticleComponent* pParticle, TextureComponent* pTexture);
	void DrawTerrain(ComponentRegistry* componentRegistry, TransformComponent* pTransform, TerrainComponent* pTerrain, TextureComponent* pTexture);
	void DrawWave(ComponentRegistry* componentRegistry, TransformComponent* pTransform, WaveComponent* pWave, TextureComponent* pTexture);

	void ControllButton();
	void DrawRenderLayerToggleUI();

	void ShadowPass(RenderPassContext renderPassContext);

	void EditorView();
	void PlayerView();

	void ResizeRenderBuffer(const Vector2& screenSize,ID3D11Texture2D** tex, ID3D11RenderTargetView** rtv, ID3D11ShaderResourceView** srv, ID3D11DepthStencilView** dsv);

	ID3D11ShaderResourceView* RenderSceneWithPostEffects(ID3D11ShaderResourceView* initialSRV, const RenderPassContext& renderPassContext);

	SceneManagerContext* m_context;

	MeshRendererComponent* m_billBoardMesh = nullptr;
	MeshRendererComponent* m_SpriteMesh = nullptr;

	ID3D11Texture2D* tex_shadow = nullptr;
	ID3D11RenderTargetView* rtv_shadow = nullptr;
	ID3D11ShaderResourceView* srv_shadow = nullptr;
	ID3D11DepthStencilView* dsv_shadow = nullptr;

	ID3D11Texture2D* tex_player = nullptr;
	ID3D11RenderTargetView* rtv_player = nullptr;
	ID3D11ShaderResourceView* srv_player = nullptr;
	ID3D11DepthStencilView* dsv_player = nullptr;

	ID3D11Texture2D* tex_editor = nullptr;
	ID3D11RenderTargetView* rtv_editor = nullptr;
	ID3D11ShaderResourceView* srv_editor = nullptr;
	ID3D11DepthStencilView* dsv_editor = nullptr;

	Vector3 m_EditorCameraPosition = Vector3(0.0f, 5.0f, -20.0f);
	Vector3 m_EditorCameraRotation = Vector3(0.0f, 0.0f, 0.0f);

	Vector3 m_CameraPosition = Vector3(0.0f, 0.0f, 0.0f);

	std::shared_ptr<PixelShaderData> m_PixelShader;
	std::shared_ptr<VertexShaderData> m_VertexShader;

	std::shared_ptr<PixelShaderData> m_LinePixelShader;
	std::shared_ptr<VertexShaderData> m_LineVertexShader;

	DirectX::XMMATRIX m_CameraView = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX m_CameraProjection = DirectX::XMMatrixIdentity();
	float m_MouseWheel = 0.0f;

	bool* showPlayer = nullptr;
	bool* showEditor = nullptr;

	bool editorRenderLayerVisible[(int)RenderLayer::MaxRenderLayer] = {
	true, true, true, true, true, true
	};

	bool playerRenderLayerVisible[(int)RenderLayer::MaxRenderLayer] = {
	true, true, true, true, true, false
	};

	int currentSelectedLayer = (int)RenderLayer::Opaque3D; // 初期選択（例）

	ID3D11Buffer* pPhysicsDebugLineVB = nullptr;
	PostEffectShader copyShader;

	std::shared_ptr<TextureData> PlayButtonTexture;
	std::shared_ptr<TextureData> PauseButtonTexture;
	std::shared_ptr<TextureData> StopButtonTexture;
	std::shared_ptr<TextureData> StepButtonTexture;

	bool mouseOnEditor = false;
};
