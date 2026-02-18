// Scene/System/renderSystem.h
#pragma once
#include "Interface/ISystem.h"
#include <d3d11.h>
#include <wrl/client.h> 
#include <string>
#include <vector>
#include <memory>
#include <mutex>

#include "Backends/myVector2.h"
#include "Backends/myVector3.h"
#include "Scene/Entity/Entity.h"

#include "System/Render/RenderSystem/renderLayer.h"

struct SceneManagerContext;
struct PixelShaderData;
struct VertexShaderData;
struct RenderableContext;
struct RenderTarget;
struct CameraEntityData;
struct TextureData;

class IRenderable;
class IRenderPass;

class PlayerPass;
class EditorPass;

class ComponentRegistry;
class TransformComponent;

class PostEffectShader;

struct PostEffect {

	PostEffectShader* shader;
	std::string name;
	bool enabled;
};

class RenderSystem : public ISystem{
public:

	const char* GetSystemName() const override{
		return "RenderSystem";
	}

	RenderSystem(SceneManagerContext* context): m_context(context){}
	~RenderSystem(){}

	void Initialize() override;
	void Finalize()override;

	void Update(float deltaTime) override;
	void EditorUpdate(float deltaTime) override;
	void Draw() override;

	void SystemSetting() override;

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

	PlayerPass* m_PlayerPass = nullptr;
	bool* showPlayer = nullptr;
	bool playerRenderLayerVisible[(int)RenderLayer::MaxRenderLayer] = {
	true, true, true, true, true,true, false
	};

	EditorPass* m_EditorPass = nullptr;
	bool* showEditor = nullptr;
	bool editorRenderLayerVisible[(int)RenderLayer::MaxRenderLayer] = {
	true, true, true, true, true, true, true,
	};

	void ReCompilePixelShaders();

	PixelShaderData* GetDefferredPS() {
		return DefferredPS.get();
	}
	PixelShaderData* GetForwardPS() {
		return ForwardPS.get();
	}

private:

	const CameraEntityData FindCameraEntity();

	void ControllButton();
	void DrawRenderLayerToggleUI();

	void EditorView();
	void PlayerView();

	SceneManagerContext* m_context;

	std::vector<std::shared_ptr<IRenderable>> m_renderables;

	PostEffectShader* copyShader = nullptr;

	std::shared_ptr<TextureData> PlayButtonTexture;
	std::shared_ptr<TextureData> PauseButtonTexture;
	std::shared_ptr<TextureData> StopButtonTexture;
	std::shared_ptr<TextureData> StepButtonTexture;

	std::shared_ptr<PixelShaderData> DefferredPS;
	std::shared_ptr<PixelShaderData> ForwardPS;
};
