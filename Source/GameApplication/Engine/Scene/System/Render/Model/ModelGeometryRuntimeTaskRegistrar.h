#pragma once

#include "Graphics/graphicsContext.h"
#include "Interface/ISystem.h"
#include "Resources/Data/modelData.h"
#include "Scene/Component/modelRendererComponent.h"
#include "System/Render/Model/ModelGeometryRuntimeStorage.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"

namespace ModelGeometryRuntimeTaskRegistrar {

inline SystemAccess BuildAccess(){
	SystemAccess access;
	access
		.ReadComponent<ModelRendererComponent>()
		.ReadResource<ModelData>()
		.ReadResource<RenderPacketFrameBuffer>()
		.WriteResource<ModelGeometryRuntimeStorage>()
		.WriteResource<GraphicsContext>();
	return access;
}

template<typename RenderSystemT>
void Register(
	RenderSystemT& system,
	SystemScheduleBuilder& builder
){
	builder.AddTask(
		"RenderSystem.ModelGeometry.Synchronize",
		SystemTaskDomain::Render,
		SystemPhase::Default,
		-100,
		BuildAccess(),
		ThreadAffinity::MainThread,
		[&system](const SystemTaskContext&){
			system.SynchronizeModelGeometryRuntime();
		}
	);
}

} // namespace ModelGeometryRuntimeTaskRegistrar
