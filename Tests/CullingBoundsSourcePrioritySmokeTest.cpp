#include <atomic>
#include <cassert>

#include "Engine/Scene/System/Render/Culling/MeshCullingBoundsProvider.h"
#include "Engine/Scene/System/Render/Culling/ModelCullingBoundsProvider.h"
#include "Engine/Scene/System/Render/Culling/TerrainCullingBoundsProvider.h"
#include "Engine/Scene/System/Render/Culling/WaveCullingBoundsProvider.h"

std::atomic<ComponentTypeID> ComponentType::s_nextID{0};

int main(){
	EntityRegistry entities;
	SceneContext context{};
	ComponentRegistry components(&entities, &context);
	context.entity = &entities;
	context.component = &components;

	const Entity entity = entities.Create();
	assert(entity);

	auto* culling = components.AddComponent<CullingComponent>(entity);
	auto* model = components.AddComponent<ModelRendererComponent>(entity);
	auto* mesh = components.AddComponent<MeshRendererComponent>(entity);
	auto* terrain = components.AddComponent<TerrainComponent>(entity);
	auto* wave = components.AddComponent<WaveComponent>(entity);
	assert(culling && model && mesh && terrain && wave);

	// Model resourceなし、Mesh metadataなし、Terrain無効ならWaveへFallbackする。
	terrain->Scale = 0;
	wave->Resolution = 8;
	wave->Amplitude = 2.0f;
	ModelCullingBoundsProvider::UpdateScene(components);
	MeshCullingBoundsProvider::UpdateScene(components);
	TerrainCullingBoundsProvider::UpdateScene(components);
	const auto waveFallback = WaveCullingBoundsProvider::UpdateScene(components);
	assert(waveFallback.updated == 1);
	assert(culling->sourceRevision ==
		WaveCullingBoundsProvider::MakeSourceRevision(*wave));
	assert(culling->localBounds.min.y == -2.0f);
	assert(culling->localBounds.max.y == 2.0f);

	// Terrainが有効になればWaveより優先する。
	terrain->Scale = 1;
	terrain->CurrentScale = 1;
	terrain->HeightMap = {0.0f, 3.0f, -1.0f, 2.0f};
	const auto terrainUpdate = TerrainCullingBoundsProvider::UpdateScene(components);
	const auto waveSkipped = WaveCullingBoundsProvider::UpdateScene(components);
	assert(terrainUpdate.updated == 1);
	assert(waveSkipped.skippedHigherPrioritySource == 1);
	assert(culling->sourceRevision ==
		TerrainCullingBoundsProvider::MakeSourceRevision(*terrain));
	assert(culling->localBounds.min.y == -1.0f);
	assert(culling->localBounds.max.y == 3.0f);

	// Mesh metadataが有効になればTerrainより優先する。
	mesh->mesh.SetLocalBounds(
		Vector3(-4.0f, -5.0f, -6.0f),
		Vector3(7.0f, 8.0f, 9.0f)
	);
	const auto meshUpdate = MeshCullingBoundsProvider::UpdateScene(components);
	const auto terrainSkipped = TerrainCullingBoundsProvider::UpdateScene(components);
	assert(meshUpdate.updated == 1);
	assert(terrainSkipped.skippedHigherPrioritySource == 1);
	assert(culling->sourceRevision ==
		MeshCullingBoundsProvider::MakeSourceRevision(*mesh));
	assert(culling->localBounds.min.x == -4.0f);
	assert(culling->localBounds.max.z == 9.0f);
	return 0;
}
