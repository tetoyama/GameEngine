#include <cassert>
#include <vector>

#include "Engine/Scene/Registry/componentRegistry.h"
#include "Engine/Scene/Registry/entityRegistry.h"
#include "Engine/Scene/System/Render/Culling/CullingVisibilityBuilder.h"

int main(){
	EntityRegistry entities;
	SceneContext context{};
	ComponentRegistry components(&entities, &context);
	context.entity = &entities;
	context.component = &components;
	context.contextID = 17;

	const CullingFrustum frustum = CullingMath::MakeAxisAlignedFrustum({
		Vector3(-10.0f, -10.0f, -10.0f),
		Vector3(10.0f, 10.0f, 10.0f)
	});

	std::vector<Entity> candidates;
	for(int index = 0; index < 32; ++index){
		const Entity entity = entities.Create();
		assert(entity);
		candidates.push_back(entity);
		auto* culling = components.AddComponent<CullingComponent>(entity);
		assert(culling);
		culling->boundsValid = false;
	}

	const CullingViewKey strictView{
		context.contextID,
		Entity{100, 1},
		CullingViewKind::Player,
		0
	};

	CullingVisibilitySet visibility;
	visibility.BeginFrame(1);
	CullingVisibilityBuilder::BuildView(
		visibility,
		strictView,
		frustum,
		components,
		1,
		false
	);

	assert(visibility.IsViewOverflowed(strictView));
	assert(!visibility.HasView(strictView));
	assert(visibility.VisibleCount(strictView) == 0);
	assert(visibility.GrowthEventCount(context.contextID) == 1);

	for(Entity entity : candidates){
		assert(CullingVisibilityBuilder::ShouldRender(
			visibility,
			strictView,
			entity,
			components
		));
	}
	return 0;
}
