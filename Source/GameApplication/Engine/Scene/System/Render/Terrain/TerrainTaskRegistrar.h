// =======================================================================
//
// TerrainTaskRegistrar.h
//
// Step 17-D: RenderSystemAnimationTaskRegistrar相当。
// TerrainSystemのメッシュ生成を2タスクへ分離して登録する。
//   - TerrainSystem.Mesh.Build  : Render / Earliest / AnyWorker（純CPU）
//   - TerrainSystem.Mesh.Upload : Render / Early    / MainThread（GPU確保）
// Phase順 Earliest→Early で Build→Upload の実行順を保証する
// （17-C の Pose.Calculate=Earliest / Upload=Early と同型）。
//
// =======================================================================
#pragma once

#include <utility>

#include "Graphics/graphicsContext.h"
#include "Interface/ISystem.h"
#include "Scene/sceneManager.h"
#include "Scene/Component/terrainComponent.h"
#include "Scene/Component/ColliderComponent.h"

namespace TerrainTaskRegistrar {

template<typename TerrainSystemT>
void Register(
	TerrainSystemT& system,
	SystemScheduleBuilder& builder
){
	// --- Build（CPU / AnyWorker） ---
	SystemAccess buildAccess;
	buildAccess
		.WriteComponent<TerrainComponent>()
		.ReadResource<SceneManager>();

	builder.AddTask(
		"TerrainSystem.Mesh.Build",
		SystemTaskDomain::Render,
		SystemPhase::Earliest,
		0,
		std::move(buildAccess),
		ThreadAffinity::AnyWorker,
		[&system](const SystemTaskContext&){
			system.BuildTerrainMeshes();
		}
	);

	// --- Upload（GPU / MainThread） ---
	SystemAccess uploadAccess;
	uploadAccess
		.WriteComponent<TerrainComponent>()
		.WriteComponent<ColliderComponent>()
		.ReadResource<SceneManager>()
		.WriteResource<GraphicsContext>();

	builder.AddTask(
		"TerrainSystem.Mesh.Upload",
		SystemTaskDomain::Render,
		SystemPhase::Early,
		0,
		std::move(uploadAccess),
		ThreadAffinity::MainThread,
		[&system](const SystemTaskContext&){
			system.UploadTerrainMeshes();
		}
	);
}

} // namespace TerrainTaskRegistrar
