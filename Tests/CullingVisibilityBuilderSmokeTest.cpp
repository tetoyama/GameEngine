#include <cassert>

#include "Engine/Scene/Registry/componentRegistry.h"
#include "Engine/Scene/Registry/entityRegistry.h"
#include "Engine/Scene/System/Render/Culling/CullingVisibilityBuilder.h"

int main(){
	EntityRegistry entities;
	SceneContext context{};
	ComponentRegistry components(&entities, &context);
	context.entity = &entities;
	context.component = &components;
	context.contextID = 7;
	context.storageConfig.visibleEntityReserve = 32;

	const Entity inside = entities.Create();
	const Entity outside = entities.Create();
	const Entity invalidBounds = entities.Create();
	const Entity noCulling = entities.Create();

	auto* insideCulling = components.AddComponent<CullingComponent>(inside);
	insideCulling->worldBounds = {
		Vector3(-1.0f, -1.0f, -1.0f),
		Vector3(1.0f, 1.0f, 1.0f)
	};
	insideCulling->boundsValid = true;

	auto* outsideCulling = components.AddComponent<CullingComponent>(outside);
	outsideCulling->worldBounds = {
		Vector3(20.0f, -1.0f, -1.0f),
		Vector3(22.0f, 1.0f, 1.0f)
	};
	outsideCulling->boundsValid = true;

	auto* invalidCulling = components.AddComponent<CullingComponent>(invalidBounds);
	invalidCulling->boundsValid = false;

	const CullingFrustum frustum = CullingMath::MakeAxisAlignedFrustum({
		Vector3(-10.0f, -10.0f, -10.0f),
		Vector3(10.0f, 10.0f, 10.0f)
	});
	const CullingViewKey view{context.contextID, Entity{100, 1}};

	CullingVisibilitySet visibility;
	visibility.BeginFrame(1);
	CullingVisibilityBuilder::BuildView(
		visibility,
		view,
		frustum,
		components,
		context.storageConfig.visibleEntityReserve
	);

	assert(visibility.IsVisible(view, inside));
	assert(!visibility.IsVisible(view, outside));
	assert(visibility.IsVisible(view, invalidBounds));
	assert(CullingVisibilityBuilder::ShouldRender(
		visibility, view, noCulling, components));
	assert(!CullingVisibilityBuilder::ShouldRender(
		visibility, view, outside, components));
	return 0;
}
