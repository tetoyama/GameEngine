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
	static_assert(MAX_COMPONENTS >= 256);
	ComponentMask mask;
	assert(TrySetComponentMaskBit(mask, 0));
	assert(TestComponentMaskBit(mask, 0));
	assert(TryResetComponentMaskBit(mask, 0));
	assert(!TestComponentMaskBit(mask, 0));
	assert(TrySetComponentMaskBit(
		mask,
		static_cast<ComponentTypeID>(MAX_COMPONENTS - 1)
	));
	assert(TestComponentMaskBit(
		mask,
		static_cast<ComponentTypeID>(MAX_COMPONENTS - 1)
	));
	assert(!TrySetComponentMaskBit(
		mask,
		static_cast<ComponentTypeID>(MAX_COMPONENTS)
	));
	assert(!TryResetComponentMaskBit(
		mask,
		static_cast<ComponentTypeID>(MAX_COMPONENTS)
	));
	assert(!TestComponentMaskBit(
		mask,
		static_cast<ComponentTypeID>(MAX_COMPONENTS)
	));

	EntityRegistry entities;
	entities.Reserve(2);
	const uint64_t initialEntityVersion = entities.GetStructureVersion();
	const Entity firstEntity = entities.Create();
	const Entity secondEntity = entities.Create();
	assert(firstEntity);
	assert(secondEntity);
	assert(entities.GetStructureVersion() == initialEntityVersion + 2);

	ComponentRegistry components(&entities, nullptr);
	const uint64_t initialComponentVersion =
		components.GetRegistryStructureVersion();
	auto* first = components.AddComponent<RegistryDenseComponent>(
		firstEntity,
		RegistryDenseComponent{10}
	);
	assert(first != nullptr);
	assert(first->value == 10);
	assert(components.HasComponent<RegistryDenseComponent>(firstEntity));
	assert(components.GetRegistryStructureVersion() == initialComponentVersion + 1);
	const uint64_t firstAddVersion = components.GetRegistryStructureVersion();

	auto query = components.ReadQuery<RegistryDenseComponent>();
	uint32_t queryCount = 0;
	for(Entity entity : query){
		(void)entity;
		++queryCount;
	}
	assert(queryCount == 1);
	assert(components.GetRegistryStructureVersion() == firstAddVersion);
	assert(entities.GetStructureVersion() == initialEntityVersion + 2);

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
	assert(components.GetRegistryStructureVersion() == firstAddVersion);

	components.RemoveComponent<RegistryDenseComponent>(firstEntity);
	assert(components.GetRegistryStructureVersion() == firstAddVersion + 1);
	auto* reused = components.AddComponent<RegistryDenseComponent>(
		secondEntity,
		RegistryDenseComponent{30}
	);
	assert(reused != nullptr);
	assert(reused->value == 30);
	assert(components.HasComponent<RegistryDenseComponent>(secondEntity));
	assert(!components.HasComponent<RegistryDenseComponent>(firstEntity));
	assert(components.GetComponentStorageCapacity<RegistryDenseComponent>() == capacityBefore);
	assert(components.GetRegistryStructureVersion() == firstAddVersion + 2);

	auto* lateFirst = components.AddComponent<LateRegisteredDenseComponent>(
		firstEntity,
		LateRegisteredDenseComponent{40}
	);
	assert(lateFirst != nullptr);
	const uint64_t lateFirstVersion = components.GetRegistryStructureVersion();
	auto* lateRejected = components.AddComponent<LateRegisteredDenseComponent>(
		secondEntity,
		LateRegisteredDenseComponent{50}
	);
	assert(lateRejected == nullptr);
	assert(!components.HasComponent<LateRegisteredDenseComponent>(secondEntity));
	assert(components.GetRegistryStructureVersion() == lateFirstVersion);

	components.SetRuntimeGrowthAllowed(true);
	auto* grown = components.AddComponent<LateRegisteredDenseComponent>(
		secondEntity,
		LateRegisteredDenseComponent{60}
	);
	assert(grown != nullptr);
	assert(grown->value == 60);
	assert(components.HasComponent<LateRegisteredDenseComponent>(secondEntity));
	assert(components.GetTotalComponentStorageGrowthEventCount() > growthBefore);
	assert(components.GetRegistryStructureVersion() == lateFirstVersion + 1);
	return 0;
}
