#pragma once

#include <utility>

#include "Interface/ISystem.h"
#include "Scene/sceneManager.h"
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
#include "System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"

namespace RenderWorldExtractionTaskRegistrar {

inline SystemAccess BuildAccess(){
	SystemAccess access;
	access
		.ReadComponent<TransformComponent>()
		.ReadComponent<RenderLayerComponent>()
		.ReadComponent<OrderInLayerComponent>()
		.ReadComponent<MaterialComponent>()
		.ReadComponent<TextureComponent>()
		.ReadComponent<ModelRendererComponent>()
		.ReadComponent<MeshRendererComponent>()
		.ReadComponent<SpriteRendererComponent>()
		.ReadComponent<BillBoardRendererComponent>()
		.ReadComponent<ParticleComponent>()
		.ReadComponent<TerrainComponent>()
		.ReadComponent<WaveComponent>()
		.ReadComponent<EffectComponent>()
		.ReadComponent<EnvironmentMapComponent>()
		.ReadResource<SceneManager>()
		.WriteResource<RenderPacketFrameBuffer>();
	return access;
}

template<typename RenderSystemT>
void Register(
	RenderSystemT& system,
	SystemScheduleBuilder& builder
){
	builder.AddTask(
		"RenderSystem.Packet.Build",
		SystemTaskDomain::Render,
		SystemPhase::Early,
		0,
		BuildAccess(),
		ThreadAffinity::AnyWorker,
		[&system](const SystemTaskContext&){
			system.BuildRenderPackets();
		}
	);
}

} // namespace RenderWorldExtractionTaskRegistrar
