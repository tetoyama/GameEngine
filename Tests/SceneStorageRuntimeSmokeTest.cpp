#include <cassert>
#include <cstddef>
#include <type_traits>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100 4471)
#endif

#include "Engine/Scene/Config/SceneStorageRuntime.h"
#include "Engine/Scene/Registry/entityRegistry.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace {

struct ComponentReserveRecorder {
	size_t transformReserve = 0;
	size_t cullingReserve = 0;
	size_t transformPages = 0;
	size_t disabledPages = 0;
	size_t staticPages = 0;
	size_t hiddenPages = 0;

	template<typename T>
	bool ReserveComponentStorage(size_t count){
		if constexpr(std::is_same_v<T, TransformComponent>){
			transformReserve = count;
		} else if constexpr(std::is_same_v<T, CullingComponent>){
			cullingReserve = count;
		}
		return true;
	}

	template<typename T>
	bool PreallocateComponentPages(size_t count){
		if constexpr(std::is_same_v<T, TransformComponent>){
			transformPages = count;
		} else if constexpr(std::is_same_v<T, DisabledComponent>){
			disabledPages = count;
		} else if constexpr(std::is_same_v<T, StaticEntityComponent>){
			staticPages = count;
		} else if constexpr(std::is_same_v<T, HiddenComponent>){
			hiddenPages = count;
		}
		return true;
	}
};

} // namespace

int main(){
	SceneStorageConfig config;
	config.expectedEntityCount = 1000;
	config.expectedTransformCount = 513;
	config.expectedCullingCount = 400;
	config.expectedStaticEntityCount = 257;

	EntityRegistry registry;
	ComponentReserveRecorder components;
	SceneStorageRuntime::Apply(registry, components, config);

	assert(registry.GetCapacity() >= 1000);
	assert(registry.GetGrowthEventCount() == 0);
	assert(components.transformReserve == 513);
	assert(components.cullingReserve == 400);
	assert(components.transformPages == 3);
	assert(components.disabledPages == 2);
	assert(components.staticPages == 2);
	assert(components.hiddenPages == 2);

	SceneStorageConfig clamped;
	clamped.expectedEntityCount = 10;
	clamped.expectedTransformCount = 100;
	clamped.expectedCullingCount = 50;
	clamped.expectedStaticEntityCount = 20;

	EntityRegistry clampedRegistry;
	ComponentReserveRecorder clampedComponents;
	SceneStorageRuntime::Apply(clampedRegistry, clampedComponents, clamped);

	assert(clampedComponents.transformReserve == 10);
	assert(clampedComponents.cullingReserve == 10);
	assert(clampedComponents.transformPages == 1);
	assert(clampedComponents.staticPages == 1);
	return 0;
}
