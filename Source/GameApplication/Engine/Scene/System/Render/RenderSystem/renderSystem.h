// =======================================================================
// 
// renderSystem.h
// 
// =======================================================================
#pragma once

#include "Interface/ISystem.h"

#include <cstdint>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "Backends/myVector2.h"
#include "Backends/myVector3.h"
#include "Scene/Entity/Entity.h"
#include "Shader/common.hlsl"
#include "System/Render/RenderSystem/renderLayer.h"
#include "System/Render/RenderSystem/ShaderMaterialProvider.h"
#include "System/Render/RenderSystem/RenderWorld/RenderWorld.h"
#include "System/Render/RenderSystem/RenderPass/RenderPassContext.h"
#include "System/Render/Animation/ModelRendererGpuRuntimeStorage.h"
#include "System/Render/Model/ModelGeometryRuntimeStorage.h"

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
// RenderSystem
// 描画処理全体を管理するシステム
//======================================================================
class RenderSystem: public ISystem, public IShaderMaterialProvider
{
private:
	// Step 18-Aの隔離済みLegacy実装だけをコンパイルするための互換Facade。
	// Active Build / Submit経路はRenderWorld APIを直接使用する。
	class RenderWorldPacketCompatibility final {
	public:
		explicit RenderWorldPacketCompatibility(RenderWorld& world) noexcept
			: m_world(world){
		}

		void BeginFrame(std::uint64_t generation){
			m_world.BeginFrame(generation);
		}

		void Merge(std::span<const RenderPacketWorkerBuffer> workerBuffers){
			m_world.Publish(workerBuffers);
		}

		std::uint64_t Generation() const noexcept {
			return m_world.Generation();
		}

	private:
		RenderWorld& m_world;
	};

	// 隔離済みLegacy Submit式をコンパイルするためだけの互換Facade。
	class RenderWorldSubmissionCompatibility final {
	public:
		explicit RenderWorldSubmissionCompatibility(RenderWorld& world) noexcept
			: m_world(world){
		}

		RenderWorldSubmissionCompatibility& operator=(
			std::uint64_t generation
		) noexcept {
			m_world.RecordSubmittedGeneration(generation);
			return *this;
		}

		operator std::uint64_t() const noexcept {
			return m_world.LastSubmittedGeneration();
		}

	private:
		RenderWorld& m_world;
	};

public:
	const char* GetSystemName() const override{
		return "RenderSystem";
	}

	RenderSystem(SceneManagerContext* context)
		: m_context(context),
		  m_renderPacketBuffer(m_renderWorld),
		  m_lastSubmittedPacketGeneration(m_renderWorld){
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

	// RenderPacketはComponentへの非所有Pointerを保持するため、
	// SceneのTempLoad / Shutdown前に公開済みRenderWorldを必ず無効化する。
	void Stop() override {
		m_renderWorld.Reset();
		m_modelRendererGpuRuntime.Reset();
		m_modelGeometryRuntime.Reset();
	}

	void Update(float deltaTime);

	// Step 17-C: CPU Pose計算とD3D11 Uploadを分離した実行段階。
	// RegisterTasksがRenderSystemAnimationTaskRegistrarで直接登録する
	// （旧EditorUpdate一体処理とMigrateRegisteredTasks互換Hookは撤去済み）。
	void CalculateAnimationPoses();
	void UploadAnimationPoses(float deltaTime);

	void Draw();
	void BuildRenderPackets();
	void SynchronizeModelGeometryRuntime();
	void SubmitRenderPackets();

	void RegisterTasks(SystemScheduleBuilder& builder) override;

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

	const CbLightingDebug& GetLightingDebugSettings() const noexcept {
		return m_lightingDebugSettings;
	}

	CbLightingDebug& GetLightingDebugSettings() noexcept {
		return m_lightingDebugSettings;
	}

	std::shared_ptr<TextureData> GetEnvironmentMap() const;
	ID3D11SamplerState* GetEnvMapSampler() const;
	IRenderable* GetRenderableForPacketKind(RenderPacketKind kind);

	RenderWorld& GetRenderWorld() noexcept {
		return m_renderWorld;
	}

	const RenderWorld& GetRenderWorld() const noexcept {
		return m_renderWorld;
	}

	ModelRendererGpuRuntimeStorage& GetModelRendererGpuRuntime() noexcept {
		return m_modelRendererGpuRuntime;
	}

	const ModelRendererGpuRuntimeStorage& GetModelRendererGpuRuntime() const noexcept {
		return m_modelRendererGpuRuntime;
	}

	ModelGeometryRuntimeStorage& GetModelGeometryRuntime() noexcept {
		return m_modelGeometryRuntime;
	}

	const ModelGeometryRuntimeStorage& GetModelGeometryRuntime() const noexcept {
		return m_modelGeometryRuntime;
	}

	RenderPacketFrameBuffer& GetRenderPacketBuffer() noexcept {
		return m_renderWorld.Packets();
	}

	const RenderPacketFrameBuffer& GetRenderPacketBuffer() const noexcept {
		return m_renderWorld.Packets();
	}

	void PrepareRenderPacketView(const RenderPacketCullingView& view){
		m_renderWorld.PrepareView(view);
	}

	void PrepareRenderPacketView(const RenderPassContext& context){
		RenderPacketCullingView view;
		view.camera = context.cameraData.ref.GetEntityID();
		view.kind = context.cullingViewKind;
		view.instanceID = context.cullingViewInstanceID;
		view.viewProjection = context.viewMatrix * context.projectionMatrix;
		PrepareRenderPacketView(view);
	}

	bool ShouldRenderPacket(
		const RenderPacketCullingView& view,
		const RenderPacket& packet
	) const {
		return m_renderWorld.ShouldRender(view, packet);
	}

	bool ShouldRenderPacket(
		const RenderPassContext& context,
		const RenderPacket& packet
	) const {
		RenderPacketCullingView view;
		view.camera = context.cameraData.ref.GetEntityID();
		view.kind = context.cullingViewKind;
		view.instanceID = context.cullingViewInstanceID;
		view.viewProjection = context.viewMatrix * context.projectionMatrix;
		return ShouldRenderPacket(view, packet);
	}

	const CullingVisibilitySet& GetCullingVisibility() const noexcept {
		return m_renderWorld.Visibility();
	}

	std::uint64_t GetLastSubmittedPacketGeneration() const noexcept {
		return m_renderWorld.LastSubmittedGeneration();
	}

	std::span<const ShaderMaterial> GetShaderMaterials() const noexcept override {
		return ShaderMaterials;
	}

	std::vector<ShaderMaterial> ShaderMaterials;

private:
	const CameraEntityData FindCameraEntity();
	void UpdateSkyBoxEnvironmentMap();
	void ControlButton();
	void DrawRenderLayerToggleUI();
	void EditorView();
	void PlayerView();

	// RenderSystemLegacyImplementation.inlへ隔離した旧実装。
	// Active Taskからは呼ばず、次工程でFacadeと一緒に削除する。
	void BuildRenderPacketsLegacy();
	void SubmitRenderPacketsLegacy();
	void RegisterTasksLegacy(SystemScheduleBuilder& builder);

private:
	SceneManagerContext* m_context = nullptr;
	std::vector<std::shared_ptr<IRenderable>> m_renderables;
	PostEffectShader* copyShader = nullptr;
	std::string ShaderPath = "Source/Shader/AutoGen";

	std::shared_ptr<TextureData> PlayButtonTexture;
	std::shared_ptr<TextureData> PauseButtonTexture;
	std::shared_ptr<TextureData> StopButtonTexture;
	std::shared_ptr<TextureData> StepButtonTexture;

	std::vector<std::shared_ptr<PixelShaderData>> DeferredPSList;
	std::vector<std::shared_ptr<PixelShaderData>> ForwardPSList;
	std::shared_ptr<PixelShaderData> ForwardPSDebug;
	CbLightingDebug m_lightingDebugSettings{};

	// Step 18-A: Frame-local描画データの所有権をRenderWorldへ集約する。
	RenderWorld m_renderWorld;

	// ModelRendererComponentから分離したEntity単位の動的Vertex Buffer Runtime。
	ModelRendererGpuRuntimeStorage m_modelRendererGpuRuntime;

	// ModelData単位の共有Static Vertex / Index RHI Geometry Runtime。
	ModelGeometryRuntimeStorage m_modelGeometryRuntime;

	// 隔離済みLegacy実装のコンパイル互換だけに残す。Active経路は使用しない。
	RenderWorldPacketCompatibility m_renderPacketBuffer;
	std::uint64_t m_renderPacketGeneration = 0;
	RenderWorldSubmissionCompatibility m_lastSubmittedPacketGeneration;

	float lazyTimer = 0.0f;
};

#include "../Animation/RenderSystemAnimationTasks.inl"
