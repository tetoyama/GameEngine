#include <atomic>
#include <cassert>

#include "Engine/Scene/System/Render/Culling/MeshCullingBoundsProvider.h"
#include "Engine/Scene/System/Render/Culling/TerrainCullingBoundsProvider.h"

std::atomic<ComponentTypeID> ComponentType::s_nextID{0};

int main(){
	EntityRegistry entities;
	SceneContext context{};
	ComponentRegistry components(&entities, &context);
	context.entity = &entities;
	context.component = &components;

	const Entity entity = entities.Create();
	assert(entity);

	auto* mesh = components.AddComponent<MeshRendererComponent>(entity);
	auto* culling = components.AddComponent<CullingComponent>(entity);
	assert(mesh && culling);

	mesh->mesh.SetLocalBounds(
		Vector3(-1.0f, -2.0f, -3.0f),
		Vector3(4.0f, 5.0f, 6.0f)
	);
	const auto meshUpdate = MeshCullingBoundsProvider::UpdateScene(components);
	assert(meshUpdate.updated == 1);
	assert(culling->sourceRevision != 0);
	assert(culling->localBounds.min.x == -1.0f);
	assert(culling->localBounds.max.z == 6.0f);

	culling->boundsValid = true;
	mesh->mesh.ClearLocalBounds();
	const auto clearedUpdate = MeshCullingBoundsProvider::UpdateScene(components);
	assert(clearedUpdate.unavailable == 1);
	assert(culling->sourceRevision == 0);
	assert(!culling->boundsValid);

	auto* terrain = components.AddComponent<TerrainComponent>(entity);
	assert(terrain);
	terrain->Scale = 1;
	terrain->CurrentScale = 1;
	terrain->HeightMap = {0.0f, 1.0f, -2.0f, 3.0f};

	MeshCullingBoundsProvider::UpdateScene(components);
	const auto terrainUpdate = TerrainCullingBoundsProvider::UpdateScene(components);
	assert(terrainUpdate.updated == 1);
	assert(culling->sourceRevision ==
		TerrainCullingBoundsProvider::MakeSourceRevision(*terrain));
	assert(culling->localBounds.min.y == -2.0f);
	assert(culling->localBounds.max.y == 3.0f);
	return 0;
}
