// =======================================================================
// 
// renderSystem.h
// 
// =======================================================================
#pragma once

#include "Interface/ISystem.h"

#include <d3d11.h>
#include <d3dcompiler.h>
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

//======================================================================
// ポストエフェクト情報
//======================================================================
struct PostEffect
{
	PostEffectShader* shader;
	std::string       name;
	bool              enabled;
};

//======================================================================
// シェーダーマテリアル定義
//======================================================================
struct ShaderMaterial
{
	std::string filePath;
	std::string entryPoint;
};

//======================================================================
// RenderSystem
// 描画処理全体を管理するシステム
//======================================================================
class RenderSystem: public ISystem
{
public:
	const char* GetSystemName() const override{
		return "RenderSystem";
	}

	RenderSystem(SceneManagerContext* context)
		: m_context(context){
		ShaderMaterials.clear();

		ShaderMaterial unlitMaterial;
		unlitMaterial.filePath = "UnlitShader.hlsli";
		unlitMaterial.entryPoint = "ShadeMaterial_Unlit";

		ShaderMaterial pbrMaterial;
		pbrMaterial.filePath = "PBRShader.hlsli";
		pbrMaterial.entryPoint = "ShadeMaterial_PBR";

		ShaderMaterials.push_back(unlitMaterial);
		ShaderMaterials.push_back(pbrMaterial);
	}

	~RenderSystem(){}

	void Initialize() override;
	void Finalize() override;

	void Update(float deltaTime);
	void EditorUpdate(float deltaTime);
	void Draw();

	void RegisterTasks(SystemScheduleBuilder& builder) override {
		builder.AddTask(
			"RenderSystem.UpdateAnimationTime",
			SystemTaskDomain::Frame,
			SystemPhase::Default,
			0,
			SystemAccess::LegacyExclusive(),
			ThreadAffinity::MainThread,
			[this](const SystemTaskContext& context){
				Update(context.deltaTime);
			}
		);

		builder.AddTask(
			"RenderSystem.EditorUpdateAnimation",
			SystemTaskDomain::Editor,
			SystemPhase::Default,
			0,
			SystemAccess::LegacyExclusive(),
			ThreadAffinity::MainThread,
			[this](const SystemTaskContext& context){
				EditorUpdate(context.deltaTime);
			}
		);

		builder.AddTask(
			"RenderSystem.Draw",
			SystemTaskDomain::Render,
			SystemPhase::Late,
			0,
			SystemAccess::LegacyExclusive(),
			ThreadAffinity::MainThread,
			[this](const SystemTaskContext&){
				Draw();
			}
		);
	}

	bool decode(const YAML::Node& node) override;
	YAML::Node encode() override;

	void SystemSetting() override;
	bool HasSystemSetting() const override { return true; }

	template<typename T>
	T* GetRenderable(){
		static_assert(std::is_base_of<IRenderable, T>::value,
			"T must inherit from IRenderable");

		for(auto& r : m_renderables){
			if(auto p = dynamic_cast<T*>(r.get())){
				return p;
			}
		}

		return nullptr;
	}

	PlayerPass* m_PlayerPass = nullptr;
	bool* showPlayer = nullptr;

	bool playerRenderLayerVisible[(int)RenderLayer::MaxRenderLayer] =
	{
		true, true, true, true, true, false
	};

	EditorPass* m_EditorPass = nullptr;
	bool* showEditor = nullptr;

	bool editorRenderLayerVisible[(int)RenderLayer::MaxRenderLayer] =
	{
		true, true, true, true, true, true,
	};

	void ReCompilePixelShaders();

	const std::vector<std::shared_ptr<PixelShaderData>>& GetDeferredPSList() const{
		return DeferredPSList;
	}

	const std::vector<std::shared_ptr<PixelShaderData>>& GetForwardPSList() const{
		return ForwardPSList;
	}

	PixelShaderData* GetForwardPSDebug() const{
		return ForwardPSDebug.get();
	}

	std::shared_ptr<TextureData> GetEnvironmentMap() const;
	ID3D11SamplerState* GetEnvMapSampler() const;

	std::vector<ShaderMaterial> ShaderMaterials;

private:
	const CameraEntityData FindCameraEntity();
	void UpdateSkyBoxEnvironmentMap();
	void ControlButton();
	void DrawRenderLayerToggleUI();
	void EditorView();
	void PlayerView();

private:
	SceneManagerContext* m_context = nullptr;
	std::vector<std::shared_ptr<IRenderable>> m_renderables;
	PostEffectShader* copyShader = nullptr;
	std::string ShaderPath = "Source/Shader/AutoGen/";

	std::shared_ptr<TextureData> PlayButtonTexture;
	std::shared_ptr<TextureData> PauseButtonTexture;
	std::shared_ptr<TextureData> StopButtonTexture;
	std::shared_ptr<TextureData> StepButtonTexture;

	std::vector<std::shared_ptr<PixelShaderData>> DeferredPSList;
	std::vector<std::shared_ptr<PixelShaderData>> ForwardPSList;
	std::shared_ptr<PixelShaderData> ForwardPSDebug;

	float lazyTimer = 0.0f;
};
