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
class IRenderPass;

class ComponentRegistry;
class TransformComponent;

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

	template<typename T>
	T* GetRenderable() {
		static_assert(std::is_base_of<IRenderable, T>::value,
						"T must inherit from IRenderable");

		for (auto& r : m_renderables) {
			if (auto p = dynamic_cast<T*>(r.get())) {
				return p;
			}
		}
		return nullptr;
	}

	template<typename T>
	T* GetRenderPass() {
		static_assert(std::is_base_of<IRenderPass, T>::value,
						"T must inherit from IRenderPass");

		for (auto& r : m_renderpass) {
			if (auto p = dynamic_cast<T*>(r.get())) {
				return p;
			}
		}
		return nullptr;
	}

private:

	const CameraEntityData FindCameraEntity();

	//void DrawEntities(const RenderableContext& renderPassContext);
	ID3D11ShaderResourceView* RenderSceneWithPostEffects(ID3D11ShaderResourceView* initialSRV, const RenderableContext& renderPassContext);

	void ControllButton();
	void DrawRenderLayerToggleUI();

	void ShadowPass(RenderableContext renderPassContext);
	void StencilShadow(RenderableContext renderPassContext);

	void EditorView();
	void PlayerView();

	SceneManagerContext* m_context;

	std::vector<std::shared_ptr<IRenderable>> m_renderables;
	std::vector<std::shared_ptr<IRenderPass>> m_renderpass;

	RenderTarget* m_RenderTargetShadow = nullptr;
	ID3D11SamplerState* shadowSampler = nullptr;

	RenderTarget* m_RenderTargetPlayer = nullptr;
	RenderTarget* m_RenderTargetEditor = nullptr;

	std::shared_ptr<PixelShaderData> m_ShadowPixelShader;

	std::shared_ptr<PixelShaderData> m_LinePixelShader;
	std::shared_ptr<VertexShaderData> m_LineVertexShader;

	bool* showEditor = nullptr;
	bool editorRenderLayerVisible[(int)RenderLayer::MaxRenderLayer] = {
	true, true, true, true, true, true
	};

	bool* showPlayer = nullptr;
	bool playerRenderLayerVisible[(int)RenderLayer::MaxRenderLayer] = {
	true, true, true, true, true, false
	};

	ID3D11Buffer* pPhysicsDebugLineVB = nullptr;

	PostEffectShader copyShader;

	std::shared_ptr<TextureData> PlayButtonTexture;
	std::shared_ptr<TextureData> PauseButtonTexture;
	std::shared_ptr<TextureData> StopButtonTexture;
	std::shared_ptr<TextureData> StepButtonTexture;

	Vector3 m_EditorCameraPosition = Vector3(0.0f, 5.0f, -20.0f);
	Vector3 m_EditorCameraRotation = Vector3(0.0f, 0.0f, 0.0f);
	float m_MouseWheel = 0.0f;
	bool mouseOnEditor = false;
};
