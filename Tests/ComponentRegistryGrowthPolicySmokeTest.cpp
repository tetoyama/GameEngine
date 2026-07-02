#include <atomic>
#include <cassert>

struct RegistryDenseComponent {
	int value = 0;
};

struct LateRegisteredDenseComponent {
	int value = 0;
};

#include "Engine/Scene/Registry/componentRegistry.h"

namespace ECSStorage {

template<>
struct ComponentStoragePreference<RegistryDenseComponent> {
	static constexpr bool HasExplicitStrategy = true;
	static constexpr ComponentStorageStrategy Strategy =
		ComponentStorageStrategy::Dense;
	static constexpr size_t ExpectedCount = 1;
	static constexpr size_t PreallocatedPages = 0;
};

template<>
struct ComponentStoragePreference<LateRegisteredDenseComponent> {
	static constexpr bool HasExplicitStrategy = true;
	static constexpr ComponentStorageStrategy Strategy =
		ComponentStorageStrategy::Dense;
	static constexpr size_t ExpectedCount = 1;
	static constexpr size_t PreallocatedPages = 0;
};

} // namespace ECSStorage

std::atomic<ComponentTypeID> ComponentType::s_nextID{0};

int main(){
	EntityRegistry entities;
	entities.Reserve(2);
	const Entity firstEntity = entities.Create();
	const Entity secondEntity = entities.Create();
	assert(firstEntity);
	assert(secondEntity);

	ComponentRegistry components(&entities, nullptr);
	auto* first = components.AddComponent<RegistryDenseComponent>(
		firstEntity,
		RegistryDenseComponent{10}
	);
	assert(first != nullptr);
	assert(first->value == 10);
	assert(components.HasComponent<RegistryDenseComponent>(firstEntity));

	components.SetRuntimeGrowthAllowed(false);
	const size_t capacityBefore =
		components.GetComponentStorageCapacity<RegistryDenseComponent>();
	const size_t growthBefore =
		components.GetTotalComponentStorageGrowthEventCount();

	auto* rejected = components.AddComponent<RegistryDenseComponent>(
		secondEntity,
		RegistryDenseComponent{20}
	);
	assert(rejected == nullptr);
	assert(!components.HasComponent<RegistryDenseComponent>(secondEntity));
	assert(components.GetComponent<RegistryDenseComponent>(secondEntity) == nullptr);
	assert(components.GetComponentStorageSize<RegistryDenseComponent>() == 1);
	assert(components.GetComponentStorageCapacity<RegistryDenseComponent>() == capacityBefore);
	assert(components.GetTotalComponentStorageGrowthEventCount() == growthBefore);

	components.RemoveComponent<RegistryDenseComponent>(firstEntity);
	auto* reused = components.AddComponent<RegistryDenseComponent>(
		secondEntity,
		RegistryDenseComponent{30}
	);
	assert(reused != nullptr);
	assert(reused->value == 30);
	assert(components.HasComponent<RegistryDenseComponent>(secondEntity));
	assert(!components.HasComponent<RegistryDenseComponent>(firstEntity));
	assert(components.GetComponentStorageCapacity<RegistryDenseComponent>() == capacityBefore);

	auto* lateFirst = components.AddComponent<LateRegisteredDenseComponent>(
		firstEntity,
		LateRegisteredDenseComponent{40}
	);
	assert(lateFirst != nullptr);
	auto* lateRejected = components.AddComponent<LateRegisteredDenseComponent>(
		secondEntity,
		LateRegisteredDenseComponent{50}
	);
	assert(lateRejected == nullptr);
	assert(!components.HasComponent<LateRegisteredDenseComponent>(secondEntity));

	components.SetRuntimeGrowthAllowed(true);
	auto* grown = components.AddComponent<LateRegisteredDenseComponent>(
		secondEntity,
		LateRegisteredDenseComponent{60}
	);
	assert(grown != nullptr);
	assert(grown->value == 60);
	assert(components.HasComponent<LateRegisteredDenseComponent>(secondEntity));
	assert(components.GetTotalComponentStorageGrowthEventCount() > growthBefore);
	return 0;
}
