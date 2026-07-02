#include <cassert>
#include <cmath>

#include "Engine/Scene/Registry/componentRegistry.h"
#include "Engine/Scene/Registry/entityRegistry.h"
#include "Engine/Scene/System/Render/Culling/CullingBoundsUpdater.h"

static bool Near(float a, float b){
	return std::fabs(a - b) < 0.0001f;
}

int main(){
	EntityRegistry entities;
	SceneContext context{};
	ComponentRegistry components(&entities, &context);
	context.entity = &entities;
	context.component = &components;

	const Entity ready = entities.Create();
	auto* transform = components.AddComponent<TransformComponent>(ready);
	transform->position = Vector3(5.0f, 0.0f, 0.0f);
	auto* culling = components.AddComponent<CullingComponent>(ready);
	culling->localBounds = {
		Vector3(-1.0f, -1.0f, -1.0f),
		Vector3(1.0f, 1.0f, 1.0f)
	};
	culling->sourceRevision = 1;

	const Entity missingSource = entities.Create();
	components.AddComponent<TransformComponent>(missingSource);
	components.AddComponent<CullingComponent>(missingSource);

	const Entity missingTransform = entities.Create();
	auto* noTransformCulling =
		components.AddComponent<CullingComponent>(missingTransform);
	noTransformCulling->localBounds = {
		Vector3(-1.0f, -1.0f, -1.0f),
		Vector3(1.0f, 1.0f, 1.0f)
	};
	noTransformCulling->sourceRevision = 1;

	const auto first = CullingBoundsUpdater::UpdateScene(components);
	assert(first.visited == 3);
	assert(first.updated == 1);
	assert(first.unavailable == 2);
	assert(culling->boundsValid);
	assert(Near(culling->worldBounds.min.x, 4.0f));
	assert(Near(culling->worldBounds.max.x, 6.0f));

	const auto unchanged = CullingBoundsUpdater::UpdateScene(components);
	assert(unchanged.updated == 0);

	transform->position = Vector3(10.0f, 0.0f, 0.0f);
	const auto moved = CullingBoundsUpdater::UpdateScene(components);
	assert(moved.updated == 1);
	assert(Near(culling->worldBounds.min.x, 9.0f));
	assert(Near(culling->worldBounds.max.x, 11.0f));
	return 0;
}
