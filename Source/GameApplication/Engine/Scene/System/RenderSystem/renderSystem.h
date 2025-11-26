// Engine/Scene/System/renderSystem.h
#pragma once
#include "Interface/ISystem.h"
#include <d3d11.h>
#include <wrl/client.h> 
#include "Backends/myVector2.h"
#include "Backends/myVector3.h"
#include "Component/RenderLayerComponent.h"
#include "GameApplication/Engine/Scene/Entity/Entity.h"

struct SceneManagerContext;

struct PixelShaderData;
struct VertexShaderData;

struct RenderableContext;
struct RenderTarget;

struct CameraEntityData;

class IRenderable;

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

	TransformComponent CalculateRectTransform(const RenderableContext& renderPassContext, const SpriteRendererComponent& sprite, const TransformComponent& transform);

	const CameraEntityData FindCameraEntity();

	void DrawEntities(const RenderableContext& renderPassContext);

	void DrawTerrain(ComponentRegistry* componentRegistry, TransformComponent* pTransform, TerrainComponent* pTerrain, TextureComponent* pTexture);
	void DrawWave(ComponentRegistry* componentRegistry, TransformComponent* pTransform, WaveComponent* pWave, TextureComponent* pTexture);

	void ControllButton();
	void DrawRenderLayerToggleUI();

	void ShadowPass(RenderableContext renderPassContext);

	void EditorView();
	void PlayerView();

	ID3D11ShaderResourceView* RenderSceneWithPostEffects(ID3D11ShaderResourceView* initialSRV, const RenderableContext& renderPassContext);

	SceneManagerContext* m_context;

	std::vector<std::shared_ptr<IRenderable>> m_renderables;

	RenderTarget* m_RenderTargetShadow = nullptr;
	RenderTarget* m_RenderTargetPlayer = nullptr;
	RenderTarget* m_RenderTargetEditor = nullptr;

	Vector3 m_EditorCameraPosition = Vector3(0.0f, 5.0f, -20.0f);
	Vector3 m_EditorCameraRotation = Vector3(0.0f, 0.0f, 0.0f);

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

	ID3D11Buffer* pPhysicsDebugLineVB = nullptr;
	PostEffectShader copyShader;

	std::shared_ptr<TextureData> PlayButtonTexture;
	std::shared_ptr<TextureData> PauseButtonTexture;
	std::shared_ptr<TextureData> StopButtonTexture;
	std::shared_ptr<TextureData> StepButtonTexture;

	bool mouseOnEditor = false;
};
