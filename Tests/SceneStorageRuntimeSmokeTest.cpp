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

void TestReserveApplication(){
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
}

void TestEntityCountClamping(){
	SceneStorageConfig clamped;
	clamped.expectedEntityCount = 10;
	clamped.expectedTransformCount = 100;
	clamped.expectedCullingCount = 50;
	clamped.expectedStaticEntityCount = 20;

	EntityRegistry registry;
	ComponentReserveRecorder components;
	SceneStorageRuntime::Apply(registry, components, clamped);

	assert(components.transformReserve == 10);
	assert(components.cullingReserve == 10);
	assert(components.transformPages == 1);
	assert(components.staticPages == 1);
}

void TestComponentlessAndTransformlessEntityReserve(){
	SceneStorageConfig config;
	config.expectedEntityCount = 1536;
	config.expectedTransformCount = 0;
	config.expectedRenderableCount = 0;
	config.expectedCullingCount = 0;
	config.expectedStaticEntityCount = 0;
	config.preallocatedTransformPages = 0;
	config.preallocatedTagPages = 0;

	EntityRegistry registry;
	ComponentReserveRecorder components;
	SceneStorageRuntime::Apply(registry, components, config);

	assert(registry.GetCapacity() >= config.expectedEntityCount);
	assert(components.transformReserve == 0);
	assert(components.transformPages == 0);

	for(size_t index = 0; index < config.expectedEntityCount; ++index){
		const Entity entity = registry.Create();
		assert(entity);
		assert(registry.IsAlive(entity));
	}

	assert(registry.GetAliveCount() == config.expectedEntityCount);
	assert(registry.GetPeakAliveCount() == config.expectedEntityCount);
	assert(registry.GetGrowthEventCount() == 0);
}

void TestEntityGrowthBeyondInitialReserve(){
	SceneStorageConfig config;
	config.expectedEntityCount = 1536;
	config.expectedTransformCount = 0;
	config.expectedRenderableCount = 0;
	config.expectedCullingCount = 0;
	config.expectedStaticEntityCount = 0;

	EntityRegistry registry;
	ComponentReserveRecorder components;
	SceneStorageRuntime::Apply(registry, components, config);

	const size_t initialCapacity = registry.GetCapacity();
	assert(initialCapacity >= config.expectedEntityCount);
	assert(registry.GetGrowthEventCount() == 0);

	const size_t targetEntityCount = initialCapacity + 1;
	for(size_t index = 0; index < targetEntityCount; ++index){
		const Entity entity = registry.Create();
		assert(entity);
		assert(registry.IsAlive(entity));
	}

	assert(registry.GetAliveCount() == targetEntityCount);
	assert(registry.GetPeakAliveCount() == targetEntityCount);
	assert(registry.GetCapacity() >= targetEntityCount);
	assert(registry.GetGrowthEventCount() > 0);
}

} // namespace

int main(){
	TestReserveApplication();
	TestEntityCountClamping();
	TestComponentlessAndTransformlessEntityReserve();
	TestEntityGrowthBeyondInitialReserve();
	return 0;
}
