#include <cassert>
#include <cstddef>
#include <cstdint>

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

struct GrowthPolicyRecorder {
	bool runtimeGrowthAllowed = true;

	template<typename T>
	bool ReserveComponentStorage(size_t){ return true; }

	template<typename T>
	bool PreallocateComponentPages(size_t){ return true; }

	void SetRuntimeGrowthAllowed(bool allowed) noexcept {
		runtimeGrowthAllowed = allowed;
	}
};

} // namespace

int main(){
	SceneStorageConfig config;
	config.expectedEntityCount = 1024;
	config.expectedTransformCount = 0;
	config.expectedRenderableCount = 0;
	config.expectedCullingCount = 0;
	config.expectedStaticEntityCount = 0;
	config.allowRuntimeGrowth = false;

	EntityRegistry registry;
	GrowthPolicyRecorder components;
	SceneStorageRuntime::Apply(registry, components, config);

	assert(!registry.IsRuntimeGrowthAllowed());
	assert(!components.runtimeGrowthAllowed);

	const size_t initialCapacity = registry.GetCapacity();
	assert(initialCapacity >= config.expectedEntityCount);
	const uint32_t rejectedIndex = static_cast<uint32_t>(initialCapacity + 1);
	const Entity requested{rejectedIndex, 0};

	assert(!registry.CreateID(requested));
	assert(registry.GetAliveCount() == 0);
	assert(registry.GetPeakAliveCount() == 0);
	assert(registry.GetGrowthEventCount() == 0);
	assert(!registry.Resolve(rejectedIndex));

	registry.SetRuntimeGrowthAllowed(true);
	const Entity resumed = registry.CreateID(requested);
	assert(resumed);
	assert(resumed.GetIndex() == rejectedIndex);
	assert(registry.IsAlive(resumed));
	assert(registry.GetAliveCount() == 1);
	assert(registry.GetPeakAliveCount() == 1);
	assert(registry.GetGrowthEventCount() > 0);
	return 0;
}
