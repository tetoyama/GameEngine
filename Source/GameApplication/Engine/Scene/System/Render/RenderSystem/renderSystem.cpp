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
#include <cstddef>
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
#include "Graphics/RHI/RHIService.h"
#include "Graphics/Portable/RenderPacketAdapter.h"
#include "Graphics/Portable/RenderMath.h"

#include "Resources/resourceService.h"
#include "Resources/Data/vertexShaderData.h"
#include "Resources/Data/pixelShaderData.h"
#include "Resources/Data/textureData.h"

static_assert(sizeof(VERTEX_3D) == 60 && offsetof(VERTEX_3D, TexCoord) == 52,
	"Update the portable vertex input layout when the existing model vertex ABI changes");

#include "Scene.h"
#include "SceneManager.h"

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"

#include "System/Physic/physicSystem.h"

#include "System/Render/RenderSystem/renderLayer.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacketTransformDX11.h"
#include "System/Render/RenderSystem/RenderWorld/RenderWorldExtraction.h"
#include "System/Render/RenderSystem/RenderWorld/RenderWorldExtractionTaskRegistrar.h"
#include "System/Render/Model/ModelGeometryRuntimeTaskRegistrar.h"

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

void RenderSystem::SynchronizeModelGeometryRuntime(){
	if(!m_context || !m_context->graphics){
		m_modelGeometryRuntime.Abandon();
		return;
	}

	RHI::RenderHardwareInterfaceService* service =
		m_context->graphics->GetRHIService();
	RHI::IRHIDevice* device = service ? service->GetDevice() : nullptr;
	const RHI::DeviceGeneration deviceGeneration = service
		? service->GetDeviceGeneration()
		: RHI::InvalidDeviceGeneration;
	if(!device || deviceGeneration == RHI::InvalidDeviceGeneration){
		m_modelGeometryRuntime.Abandon();
		return;
	}

	const RenderPacketFrameBuffer& frameBuffer = m_renderWorld.Packets();
	const std::span<const RenderPacket> packets = frameBuffer.IsReady()
		? std::span<const RenderPacket>(frameBuffer.Packets())
		: std::span<const RenderPacket>{};
	m_modelGeometryRuntime.Synchronize(
		*device,
		packets,
		m_renderWorld.Generation(),
		deviceGeneration
	);
}

void RenderSystem::SubmitRenderPackets(){
	(void)m_renderWorld.MarkSubmitted();
	Draw();
}

ID3D11ShaderResourceView* RenderSystem::RenderPortableView(const RenderPassContext& context, bool editorView){
	if(!m_context || !m_context->graphics || context.screenSize.x < 1 || context.screenSize.y < 1) return nullptr;
	auto* service = m_context->graphics->GetRHIService();
	if(!service || !service->GetDevice()) return nullptr;
	try {
		auto& viewport = editorView ? m_portableEditorView : m_portablePlayerView;
		if(!viewport) viewport = std::make_unique<Rendering::EditorGPUViewport>(*service->GetDevice(),
			m_context->graphics->GetDevice(), m_context->graphics->GetDeviceContext());
		Rendering::FrameUniforms frame;
		DirectX::XMFLOAT4X4 matrix;
		DirectX::XMStoreFloat4x4(&matrix, context.viewMatrix * context.projectionMatrix);
		std::copy_n(&matrix._11, 16, frame.viewProjection.begin());
		frame.lightViewProjection = Rendering::Multiply(Rendering::Orthographic(80,80,0.1f,200),
			Rendering::LookAt({-30,50,-30},{0,0,0}));
		frame.lightDirection = {0.4575f,-0.7625f,0.4575f,0};
		frame.lightColor = {0,0,0,0}; frame.ambientColor = {0,0,0,0};
		// Match the existing viewport's linear UNORM presentation. Camera
		// post effects are not implemented by this compatibility path yet.
		frame.outputTransform = {0,0,0,0};
		// Reuse the Scene's existing light and transform contract. This stage
		// supports one directional light with one shadow map, not a second
		// independently configured lighting service.
		std::vector<std::shared_ptr<Scene>> scenes;
		for(const auto& [name,scene]:m_context->sceneManager->GetActiveScenes()) if(scene) scenes.push_back(scene);
		std::sort(scenes.begin(),scenes.end(),[](const auto& a,const auto& b){return a->GetSceneContext()->contextID<b->GetSceneContext()->contextID;});
		bool lightFound=false;
		for(const auto& scene:scenes){
			auto* components=scene->GetSceneContext()->component;
			auto lights=components->FindEntitiesWithComponent<LightComponent>();
			std::sort(lights.begin(),lights.end());
			for(Entity entity:lights){
				const auto* light=components->GetComponent<LightComponent>(entity);
				const auto* transform=components->GetComponent<TransformComponent>(entity);
				if(!light || !transform || !light->light.Enable ||
					(light->light.LightType!=LIGHT_TYPE_DIRECTIONAL && light->light.LightType!=LIGHT_TYPE_DIRECTIONAL_CSM)) continue;
				const auto front=transform->front();
				const auto direction=Rendering::Normalize({front.x,front.y,front.z});
				frame.lightDirection={direction[0],direction[1],direction[2],0};
				frame.lightColor={light->light.Diffuse.x,light->light.Diffuse.y,light->light.Diffuse.z,light->light.CastShadow?1.f:0.f};
				frame.ambientColor={light->light.Ambient.x,light->light.Ambient.y,light->light.Ambient.z,0};
				const Rendering::Vec3 center{context.CameraPosition.x,context.CameraPosition.y,context.CameraPosition.z};
				const float size=(std::max)(50.f,light->light.Param.x/10.f);
				const Rendering::Vec3 eye{center[0]-direction[0]*size,center[1]-direction[1]*size,center[2]-direction[2]*size};
				const Rendering::Vec3 up=std::abs(direction[1])>.99f?Rendering::Vec3{0,0,1}:Rendering::Vec3{0,1,0};
				frame.lightViewProjection=Rendering::Multiply(Rendering::Orthographic(size,size,.1f,size*2),Rendering::LookAt(eye,center,up));
				lightFound=true; break;
			}
			if(lightFound) break;
		}
		std::vector<RenderPacket> visible;
		for(const auto& packet : m_renderWorld.Packets().Packets()){
			const auto layer = static_cast<size_t>(packet.layer);
			if(layer < static_cast<size_t>(RenderLayer::MaxRenderLayer) &&
				context.renderLayerVisibility[layer] && ShouldRenderPacket(context, packet)){
				visible.push_back(packet);
			}
		}
		auto conversion = Rendering::ConvertRenderPackets(visible, frame, m_renderWorld.Generation(),
			[this](const RenderPacket& packet){
				std::vector<ModelGeometryRuntimeMesh> meshes;
				// Animated geometry is not passed off as a rendered current pose.
				if(packet.bindings.modelRenderer && !packet.bindings.modelRenderer->blendedAnimations.empty()) return meshes;
				const auto* runtime = m_modelGeometryRuntime.Find(packet.modelResource.get());
				if(runtime) for(size_t index=0; index<runtime->MeshCount(); ++index){
					if(packet.TargetsAllSubMeshes() || packet.TargetsSubMesh(static_cast<uint32_t>(index)))
						if(const auto* mesh = runtime->Mesh(index)) meshes.push_back(*mesh);
				}
				return meshes;
			},[this,service](const RenderPacket& packet, Rendering::DrawItem& draw){
				const auto* material=packet.modelMaterial.GetDescriptor();
				if(material && material->shaderID!=0 && material->shaderID!=1) return false;
				if(material) draw.instance.shading[0]=material->shaderID==0?1.f:0.f;
				std::shared_ptr<TextureData> texture;
				if(packet.bindings.texture && packet.bindings.texture->m_TextureData){
					texture=packet.bindings.texture->m_TextureData;
					const auto uv=packet.bindings.texture->ResolveUVMatrixBuffer();
					draw.instance.uvTransform={uv.UVEnd.x-uv.UVStart.x,uv.UVEnd.y-uv.UVStart.y,uv.UVStart.x,uv.UVStart.y};
				}
				if(material && !texture) for(const auto& binding:material->textures){
					if(binding.semantic!=MaterialTextureSemantic::BaseColor || binding.uvChannel!=0 || binding.uvRotation!=0) return false;
					if(!texture){
						texture=m_context->resource->Load<TextureData>(binding.assetPath);
						draw.instance.uvTransform={binding.uvScale[0],binding.uvScale[1],binding.uvOffset[0],binding.uvOffset[1]};
						if(!texture) return false;
					}
				}
				if(texture){
					draw.albedoTexture=texture->EnsureRHI(*service->GetDevice(),m_context->graphics->GetDevice(),m_context->graphics->GetDeviceContext());
					if(!draw.albedoTexture) return false;
				}
				return true;
			});
		m_portableViewStatus = std::string(ToEngineConfigBackendName(service->GetSelectedBackend())) +
			" scene / DX11 editor UI; compatibility readback. Draws: " + std::to_string(conversion.scene.draws.size()) + "; unsupported: " +
			std::to_string(conversion.unsupportedPackets) + "; unresolved/animated: " + std::to_string(conversion.unresolvedMeshes);
		return viewport->Render(conversion.scene, static_cast<uint32_t>(context.screenSize.x), static_cast<uint32_t>(context.screenSize.y));
	} catch(const std::exception& error){
		const std::string status = std::string("Selected rendering API failed: ") + error.what();
		if(status != m_portableViewStatus) m_context->debug->Error(status, "RenderSystem::RenderPortableView");
		m_portableViewStatus = status;
		return nullptr;
	}
}

void RenderSystem::ResetPortableViews(){
	if(m_EditorPass) m_EditorPass->result = nullptr;
	if(m_PlayerPass) m_PlayerPass->result = nullptr;
	m_portableEditorView.reset();
	m_portablePlayerView.reset();
	m_portableViewStatus.clear();
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
	ModelGeometryRuntimeTaskRegistrar::Register(*this, builder);

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
