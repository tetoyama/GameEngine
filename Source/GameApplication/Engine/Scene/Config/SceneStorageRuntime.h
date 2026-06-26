// =======================================================================
//
// SceneStorageRuntime.h
//
// =======================================================================
#pragma once

#include "Scene/Component/CullingComponent.h"
#include "Scene/Component/EntityStateComponents.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Config/SceneStorageConfig.h"

namespace SceneStorageRuntime {

template<typename EntityRegistryT, typename ComponentRegistryT>
void Apply(
	EntityRegistryT& entityRegistry,
	ComponentRegistryT& componentRegistry,
	SceneStorageConfig config
){
	config.Normalize();

	entityRegistry.Reserve(config.expectedEntityCount);

	componentRegistry.template ReserveComponentStorage<TransformComponent>(
		config.expectedTransformCount
	);
	componentRegistry.template PreallocateComponentPages<TransformComponent>(
		config.ResolveTransformPageCount()
	);
	componentRegistry.template ReserveComponentStorage<CullingComponent>(
		config.expectedCullingCount
	);

	const size_t tagPageCount = config.ResolveTagPageCount();
	componentRegistry.template PreallocateComponentPages<DisabledComponent>(
		tagPageCount
	);
	componentRegistry.template PreallocateComponentPages<StaticEntityComponent>(
		tagPageCount
	);
	componentRegistry.template PreallocateComponentPages<HiddenComponent>(
		tagPageCount
	);

	if constexpr(requires {
		entityRegistry.SetRuntimeGrowthAllowed(config.allowRuntimeGrowth);
	}){
		entityRegistry.SetRuntimeGrowthAllowed(config.allowRuntimeGrowth);
	}
	if constexpr(requires {
		componentRegistry.SetRuntimeGrowthAllowed(config.allowRuntimeGrowth);
	}){
		componentRegistry.SetRuntimeGrowthAllowed(config.allowRuntimeGrowth);
	}
}

} // namespace SceneStorageRuntime
