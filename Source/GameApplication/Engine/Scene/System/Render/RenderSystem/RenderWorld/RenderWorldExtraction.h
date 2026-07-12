#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <DirectXMath.h>

#include "Backends/myVector3.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/componentRegistry.h"
#include "Scene/Component/RenderLayerComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Component/materialComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/BillBoardRendererComponent.h"
#include "Scene/Component/2DspriteRendererComponent.h"
#include "Scene/Component/terrainComponent.h"
#include "Scene/Component/waveComponent.h"
#include "Scene/Component/particleComponent.h"
#include "Scene/Component/EffectComponent.h"
#include "Scene/Component/textureComponent.h"
#include "Scene/Component/environmentMapComponent.h"
#include "System/Render/RenderSystem/renderLayer.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacketTransformDX11.h"
#include "System/Render/RenderSystem/RenderWorld/RenderWorld.h"

struct RenderWorldExtractionTelemetry {
	std::uint64_t generation = 0;
	std::size_t sceneCount = 0;
	std::size_t entityCount = 0;
	std::size_t packetCount = 0;
};

// ECS World / ComponentRegistryからFrame-local Render Packetを抽出する責務。
// RenderSystemはTaskの起動とGeneration更新だけを行い、Component走査を持たない。
class RenderWorldExtraction final {
public:
	RenderWorldExtraction() = delete;

	static RenderWorldExtractionTelemetry Extract(
		SceneManager* sceneManager,
		RenderWorld& renderWorld,
		std::uint64_t generation
	){
		std::array<RenderPacketWorkerBuffer, 1> workerBuffers{
			RenderPacketWorkerBuffer(0)
		};
		RenderPacketWorkerBuffer& worker = workerBuffers[0];

		RenderWorldExtractionTelemetry telemetry;
		telemetry.generation = generation;
		std::uint64_t stableSequence = 0;

		struct SceneEntry {
			std::uint32_t contextID = 0;
			std::string name;
			SceneContext* context = nullptr;
		};

		std::vector<SceneEntry> scenes;
		if(sceneManager){
			for(const auto& [sceneName, scene] : sceneManager->GetActiveScenes()){
				if(!scene) continue;
				SceneContext* context = scene->GetSceneContext();
				if(!context || !context->component || context->contextID == 0) continue;
				scenes.push_back({context->contextID, sceneName, context});
			}
		}

		std::sort(
			scenes.begin(),
			scenes.end(),
			[](const SceneEntry& lhs, const SceneEntry& rhs){
				if(lhs.contextID != rhs.contextID) return lhs.contextID < rhs.contextID;
				return lhs.name < rhs.name;
			}
		);
		telemetry.sceneCount = scenes.size();

		for(const SceneEntry& sceneEntry : scenes){
			ComponentRegistry* components = sceneEntry.context->component;
			auto entities = components->FindEntitiesWithComponent<TransformComponent>();
			std::sort(
				entities.begin(),
				entities.end(),
				[](Entity lhs, Entity rhs){
					return lhs.GetPackedValue() < rhs.GetPackedValue();
				}
			);

			telemetry.entityCount += entities.size();
			worker.Reserve(worker.Packets().size() + entities.size());
			for(Entity entity : entities){
				TransformComponent* transform =
					components->GetComponent<TransformComponent>(entity);
				if(!transform) continue;

				const RenderLayerComponent* layerComponent =
					components->GetComponent<RenderLayerComponent>(entity);
				const OrderInLayerComponent* orderComponent =
					components->GetComponent<OrderInLayerComponent>(entity);
				MaterialComponent* materialComponent =
					components->GetComponent<MaterialComponent>(entity);
				TextureComponent* textureComponent =
					components->GetComponent<TextureComponent>(entity);
				ModelRendererComponent* modelRenderer =
					components->GetComponent<ModelRendererComponent>(entity);
				MeshRendererComponent* meshRenderer =
					components->GetComponent<MeshRendererComponent>(entity);
				SpriteRendererComponent* spriteRenderer =
					components->GetComponent<SpriteRendererComponent>(entity);
				BillBoardRendererComponent* billboardRenderer =
					components->GetComponent<BillBoardRendererComponent>(entity);
				ParticleComponent* particle =
					components->GetComponent<ParticleComponent>(entity);
				TerrainComponent* terrain =
					components->GetComponent<TerrainComponent>(entity);
				WaveComponent* wave =
					components->GetComponent<WaveComponent>(entity);
				EffectComponent* effect =
					components->GetComponent<EffectComponent>(entity);
				const bool isEnvironmentMap =
					components->GetComponent<EnvironmentMapComponent>(entity) != nullptr;

				const RenderLayer layer = layerComponent
					? layerComponent->layer
					: RenderLayer::Opaque3D;
				const std::int32_t orderInLayer = orderComponent
					? orderComponent->order
					: 0;
				const std::uint32_t materialKey = materialComponent
					? static_cast<std::uint32_t>((std::max)(0, materialComponent->ShaderID))
					: 0u;

				RenderPacketTransformSnapshot snapshot;
				snapshot.position[0] = transform->position.x;
				snapshot.position[1] = transform->position.y;
				snapshot.position[2] = transform->position.z;
				const Vector3 worldPosition = transform->GetWorldPosition(components);
				snapshot.worldPosition[0] = worldPosition.x;
				snapshot.worldPosition[1] = worldPosition.y;
				snapshot.worldPosition[2] = worldPosition.z;
				const DirectX::XMFLOAT4& rotation = transform->GetRotation();
				snapshot.rotation[0] = rotation.x;
				snapshot.rotation[1] = rotation.y;
				snapshot.rotation[2] = rotation.z;
				snapshot.rotation[3] = rotation.w;
				snapshot.scale[0] = transform->scale.x;
				snapshot.scale[1] = transform->scale.y;
				snapshot.scale[2] = transform->scale.z;
				StoreRenderPacketMatrix(
					snapshot.worldMatrix,
					transform->CalculateWorldMatrix(transform, components)
				);
				if(transform->parent){
					if(TransformComponent* parentTransform =
						components->GetComponent<TransformComponent>(transform->parent)){
						StoreRenderPacketMatrix(
							snapshot.parentWorldMatrix,
							parentTransform->CalculateWorldMatrix(parentTransform, components)
						);
						snapshot.hasParentWorld = true;
					}
				}

				auto appendPacket = [&](RenderPacketKind kind){
					RenderPacket packet;
					packet.sceneContextID = sceneEntry.contextID;
					packet.entity = entity;
					packet.kind = kind;
					const RenderLayer effectiveLayer = ResolveRenderPacketLayer(layer, kind);
					packet.layer = effectiveLayer;
					packet.passMask = ResolveRenderPacketPasses(effectiveLayer, kind);
					if(isEnvironmentMap){
						packet.passMask = RemoveRenderPacketPass(
							packet.passMask,
							RenderPacketPassMask::Shadow
						);
					}
					packet.materialKey = materialKey;
					packet.orderInLayer = orderInLayer;
					packet.sortKey = MakeRenderPacketSortKey(
						effectiveLayer,
						kind,
						materialKey,
						orderInLayer
					);
					packet.stableSequence = stableSequence++;
					packet.transform = snapshot;
					packet.bindings.sceneContext = sceneEntry.context;
					packet.bindings.transform = transform;
					packet.bindings.material = materialComponent;
					packet.bindings.texture = textureComponent;
					packet.bindings.modelRenderer = modelRenderer;
					packet.bindings.meshRenderer = meshRenderer;
					packet.bindings.spriteRenderer = spriteRenderer;
					packet.bindings.billboardRenderer = billboardRenderer;
					packet.bindings.particle = particle;
					packet.bindings.terrain = terrain;
					packet.bindings.wave = wave;
					packet.bindings.effect = effect;
					worker.Add(std::move(packet));
				};

				if(modelRenderer) appendPacket(RenderPacketKind::Model);
				if(meshRenderer) appendPacket(RenderPacketKind::Mesh);
				if(spriteRenderer) appendPacket(RenderPacketKind::Sprite);
				if(billboardRenderer) appendPacket(RenderPacketKind::Billboard);
				if(particle) appendPacket(RenderPacketKind::Particle);
				if(terrain) appendPacket(RenderPacketKind::Terrain);
				if(wave) appendPacket(RenderPacketKind::Wave);
				if(effect) appendPacket(RenderPacketKind::Effect);
			}
		}

		telemetry.packetCount = worker.Size();
		Publish(renderWorld, generation, workerBuffers);
		return telemetry;
	}

	static void Publish(
		RenderWorld& renderWorld,
		std::uint64_t generation,
		std::span<const RenderPacketWorkerBuffer> workerBuffers
	){
		renderWorld.BeginFrame(generation);
		renderWorld.Publish(workerBuffers);
	}
};
