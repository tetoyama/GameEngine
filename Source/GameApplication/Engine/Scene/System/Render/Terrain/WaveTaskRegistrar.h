// =======================================================================
//
// WaveTaskRegistrar.h
//
// Step 17-E: Wave頂点更新を純CPU BuildとMainThread Uploadへ分離する。
//
// =======================================================================
#pragma once

#include <utility>

#include "Graphics/graphicsContext.h"
#include "Interface/ISystem.h"
#include "Scene/sceneManager.h"
#include "Scene/Component/waveComponent.h"

namespace WaveTaskRegistrar {

template<typename WaveSystemT>
void Register(
	WaveSystemT& system,
	SystemScheduleBuilder& builder
){
	SystemAccess buildAccess;
	buildAccess
		.WriteComponent<WaveComponent>()
		.ReadResource<SceneManager>();

	builder.AddTask(
		"WaveSystem.Vertex.Build",
		SystemTaskDomain::Render,
		SystemPhase::Earliest,
		0,
		std::move(buildAccess),
		ThreadAffinity::AnyWorker,
		[&system](const SystemTaskContext&){
			system.BuildWaveVertices();
		}
	);

	SystemAccess uploadAccess;
	uploadAccess
		.WriteComponent<WaveComponent>()
		.ReadResource<SceneManager>()
		.WriteResource<GraphicsContext>();

	builder.AddTask(
		"WaveSystem.Vertex.Upload",
		SystemTaskDomain::Render,
		SystemPhase::Early,
		0,
		std::move(uploadAccess),
		ThreadAffinity::MainThread,
		[&system](const SystemTaskContext&){
			system.UploadWaveVertices();
		}
	);
}

} // namespace WaveTaskRegistrar
