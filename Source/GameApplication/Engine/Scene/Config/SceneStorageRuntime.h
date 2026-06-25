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

// Scene初期化時のStorage確保処理を一箇所へ集約する。
// Registry型をTemplateにすることで、実RegistryとSmoke Test用Recorderの両方で検証できる。
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
}

} // namespace SceneStorageRuntime
