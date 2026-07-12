// =======================================================================
//
// renderSystem.cpp
//
// Step 18-A Active RenderWorld Build / Submit implementation.
// The unchanged legacy implementation is isolated in
// RenderSystemLegacyImplementation.inl until its compatibility facades are
// removed in the next migration unit.
//
// =======================================================================
#include "renderSystem.h"
#include "buildSetting.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <queue>
#include <thread>

#include <DirectXMath.h>

#include "Backends/DirectX11/DirectXTex.h"
#include "Backends/ImGui/ImGui.h"
#include "Backends/ImGui/ImGuizmo.h"
#include "Backends/myVector3.h"

#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"
#include "Backends/Assimp/matrix4x4.h"

#include "Backends/myMath.h"

#include "DebugTools/debugSystem.h"

#include "Resources/Data/modelData.h"

#include "Registry/entityRegistry.h"
#include "Registry/systemRegistry.h"
#include "Registry/componentRegistry.h"

#include "Graphics/graphicsContext.h"
#include "Graphics/mainRenderer.h"

#include "Resources/resourceService.h"
#include "Resources/Data/vertexShaderData.h"
#include "Resources/Data/pixelShaderData.h"
#include "Resources/Data/textureData.h"

#include "Scene.h"
#include "SceneManager.h"

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"

#include "System/Physic/physicSystem.h"

#include "System/Render/RenderSystem/renderLayer.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacketTransformDX11.h"
#include "System/Render/RenderSystem/RenderWorld/RenderWorldExtraction.h"
#include "System/Render/RenderSystem/RenderWorld/RenderWorldExtractionTaskRegistrar.h"

#include "Component/RenderLayerComponent.h"
#include "Component/transformComponent.h"
#include "Component/CameraComponent.h"
#include <Component/modelRendererComponent.h>
#include <Component/materialComponent.h>
#include <Component/meshRendererComponent.h>
#include <Component/BillBoardRendererComponent.h>
#include <Component/2DspriteRendererComponent.h>
#include <Component/terrainComponent.h>
#include <Component/waveComponent.h>
#include <Component/particleComponent.h>
#include <Component/EffectComponent.h>
#include <Component/LightComponent.h>
#include <Component/textureComponent.h>
#include <Component/environmentMapComponent.h>

#include "CameraEntityData.h"
#include "renderPhase.h"
#include "RenderTarget/renderTarget.h"
#include "Renderable/Mesh/RenderableMesh.h"
#include "Renderable/Model/RenderableModel.h"
#include "Renderable/BillBoard/RenderableBillBoard.h"
#include "Renderable/Sprite/RenderableSprite.h"
#include "Renderable/Particle/RenderableParticle.h"
#include "Renderable/Terrain/RenderableTerrain.h"
#include "RenderPass/IRenderPass.h"

#include "RenderPass/GBuffer/GBufferPass.h"
#include "RenderPass/ShadowMap/ShadowMapPass.h"
#include "RenderPass/LightingPass/LightingPass.h"
#include "RenderPass/PlayerView/PlayerPass.h"
#include "RenderPass/PlayerView/PlayerViewRefreshPolicy.h"
#include "RenderPass/EditorView/EditorPass.h"
#include "Renderable/Wave/RenderableWave.h"
#include <Editor/UI/ViewWindow.h>
#include "Renderable/Effect/RenderableEffect.h"

#include "Service/Config/configSystem.h"
#include "Service/Config/appConfig.h"
#include "Backends/ImGuiFunc.h"
#include "Editor/Command/CommandManager.h"
#include "Editor/Command/PropertyChangeCommand.h"

// Keep every dependency parsed before the migration renames below. This avoids
// changing unrelated RegisterTasks declarations while the legacy file is
// included as one translation-unit implementation block.
#define BuildRenderPackets BuildRenderPacketsLegacy
#define SubmitRenderPackets SubmitRenderPacketsLegacy
#define RegisterTasks RegisterTasksLegacy
#include "RenderSystemLegacyImplementation.inl"
#undef RegisterTasks
#undef SubmitRenderPackets
#undef BuildRenderPackets

void RenderSystem::BuildRenderPackets(){
	SceneManager* sceneManager = m_context ? m_context->sceneManager : nullptr;
	RenderWorldExtraction::Extract(
		sceneManager,
		m_renderWorld,
		++m_renderPacketGeneration
	);
}

void RenderSystem::SubmitRenderPackets(){
	(void)m_renderWorld.MarkSubmitted();
	Draw();
}

void RenderSystem::RegisterTasks(SystemScheduleBuilder& builder){
	using RenderUpdateQuery = ECSQuery::ComponentQueryView<
		ECSQuery::Read<TransformComponent>,
		ECSQuery::Write<ModelRendererComponent>
	>;

	builder.AddQueryTask<RenderUpdateQuery>(
		"RenderSystem.AnimationTime.Commit",
		SystemTaskDomain::Frame,
		SystemPhase::Late,
		0,
		StructuralAccess::None,
		ThreadAffinity::AnyWorker,
		[this](const SystemTaskContext& context){
			Update(context.deltaTime);
		}
	);

	RenderSystemAnimationTaskRegistrar::Register(*this, builder);
	RenderWorldExtractionTaskRegistrar::Register(*this, builder);

	SystemAccess submitAccess = SystemAccess::LegacyExclusive();
	submitAccess.ReadResource<RenderPacketFrameBuffer>();
	builder.AddTask(
		"RenderSystem.Command.Submit",
		SystemTaskDomain::Render,
		SystemPhase::Late,
		0,
		std::move(submitAccess),
		ThreadAffinity::MainThread,
		[this](const SystemTaskContext&){
			SubmitRenderPackets();
		}
	);
}
