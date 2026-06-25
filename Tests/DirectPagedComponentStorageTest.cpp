#include <cassert>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "Engine/Scene/Component/EntityStateComponents.h"
#include "Engine/Scene/Query/ComponentQueryView.h"
#include "Engine/Scene/Storage/ComponentStorageFactory.h"
#include "Engine/Scene/Storage/ComponentStorageStrategy.h"
#include "Engine/Scene/Storage/DirectPagedComponentStorage.h"

namespace {

struct TestComponent {
	int value = 0;
};

struct TestTag {};

} // namespace

namespace ECSStorage {

template<>
struct IsTagComponent<TestTag>: std::true_type {};

} // namespace ECSStorage

namespace {

static_assert(
	ECSStorage::ComponentStorageStrategy::DirectPaged !=
	ECSStorage::ComponentStorageStrategy::Dense
);
static_assert(!ECSStorage::IsTagComponentV<TestComponent>);
static_assert(ECSStorage::IsTagComponentV<TestTag>);
static_assert(ECSStorage::IsTagComponentV<DisabledComponent>);
static_assert(ECSStorage::IsTagComponentV<StaticEntityComponent>);
static_assert(ECSStorage::IsTagComponentV<HiddenComponent>);
static_assert(ECSStorage::IsEntityHeaderComponentV<DisabledComponent>);
static_assert(ECSStorage::ExcludeFromDefaultQueriesV<DisabledComponent>);
static_assert(!ECSStorage::ExcludeFromDefaultQueriesV<HiddenComponent>);
static_assert(
	ECSStorage::ComponentStoragePreference<DisabledComponent>::Strategy ==
	ECSStorage::ComponentStorageStrategy::DirectPaged
);

void TestStorageFactory(){
	using ECSStorage::ComponentStorageStrategy;

	auto dense = ECSStorage::CreateComponentStorage<TestComponent>(
		ComponentStorageStrategy::Dense
	);
	assert(dynamic_cast<DenseComponentPool<TestComponent>*>(dense.get()) != nullptr);

	auto directData = ECSStorage::CreateComponentStorage<TestComponent>(
		ComponentStorageStrategy::DirectPaged
	);
	assert(
		dynamic_cast<ECSStorage::DirectPagedComponentStorage<TestComponent>*>(
			directData.get()
		) != nullptr
	);
	directData->Reserve(32);
	directData->PreallocatePages(2);
	assert(directData->Capacity() == 512);

	auto directTag = ECSStorage::CreateComponentStorage<TestTag>(
		ComponentStorageStrategy::DirectPaged
	);
	assert(
		dynamic_cast<ECSStorage::DirectPagedTagStorage<TestTag>*>(
			directTag.get()
		) != nullptr
	);
	directTag->PreallocatePages(1);
	assert(directTag->Capacity() == 256);

	auto disabledTag = ECSStorage::CreateComponentStorage<DisabledComponent>(
		ECSStorage::ComponentStoragePreference<DisabledComponent>::Strategy
	);
	assert(
		dynamic_cast<ECSStorage::DirectPagedTagStorage<DisabledComponent>*>(
			disabledTag.get()
		) != nullptr
	);

	auto sparse = ECSStorage::CreateComponentStorage<TestComponent>(
		ComponentStorageStrategy::SparseStable
	);
	assert(dynamic_cast<SparseStorage<TestComponent>*>(sparse.get()) != nullptr);

	auto archetypeFallback = ECSStorage::CreateComponentStorage<TestComponent>(
		ComponentStorageStrategy::Archetype
	);
	assert(
		dynamic_cast<DenseComponentPool<TestComponent>*>(
			archetypeFallback.get()
		) != nullptr
	);
}

void TestDataStorage(){
	ECSStorage::DirectPagedComponentStorage<TestComponent, 4> storage;
	storage.ReservePageTable(32);
	assert(storage.Capacity() == 0);
	storage.PreallocatePages(2);
	assert(storage.GetAllocatedPageCount() == 2);
	assert(storage.Capacity() == 8);

	const Entity first{1, 1};
	const Entity second{6, 1};
	storage.Add(first, TestComponent{10});
	storage.Add(second, TestComponent{20});

	assert(storage.Size() == 2);
	assert(storage.Contains(first));
	assert(storage.Contains(second));
	assert(storage.Get(first)->value == 10);
	assert(storage.Get(second)->value == 20);

	TestComponent* stableAddress = storage.Get(first);
	storage.Add(Entity{15, 1}, TestComponent{30});
	assert(storage.Get(first) == stableAddress);
	assert(storage.GetAllocatedPageCount() == 3);
	assert(storage.Capacity() == 12);

	int visitedCount = 0;
	int valueTotal = 0;
	storage.ForEachOccupied([&](Entity, TestComponent& component){
		++visitedCount;
		valueTotal += component.value;
		component.value += 1;
	});
	assert(visitedCount == 3);
	assert(valueTotal == 60);
	assert(storage.Get(first)->value == 11);
	assert(storage.Get(second)->value == 21);

	const auto& constStorage = storage;
	int constVisitedCount = 0;
	constStorage.ForEachOccupied([&](Entity, const TestComponent&){
		++constVisitedCount;
	});
	assert(constVisitedCount == 3);

	const std::uint32_t generationBeforeRemove =
		storage.GetComponentGeneration(first);
	storage.Remove(first);
	assert(!storage.Contains(first));
	assert(storage.GetComponentGeneration(first) != generationBeforeRemove);
	assert(storage.Capacity() == 12);

	const Entity reusedIndex{1, 2};
	storage.Add(reusedIndex, TestComponent{40});
	assert(!storage.Contains(first));
	assert(storage.Contains(reusedIndex));
	assert(storage.Get(reusedIndex)->value == 40);
}

void TestTagStorage(){
	ECSStorage::DirectPagedTagStorage<TestTag, 4> storage;
	assert(storage.Capacity() == 0);
	const Entity first{2, 1};
	const Entity reusedIndex{2, 2};

	storage.Add(first);
	assert(storage.Contains(first));
	assert(storage.GetRaw(first) != nullptr);
	assert(storage.GetAllocatedPageCount() == 1);
	assert(storage.Capacity() == 4);

	storage.Add(reusedIndex);
	assert(!storage.Contains(first));
	assert(storage.Contains(reusedIndex));
	assert(storage.Size() == 1);
	assert(storage.Capacity() == 4);

	int visitedCount = 0;
	Entity visitedEntity{};
	storage.ForEachEntity([&](Entity entity){
		++visitedCount;
		visitedEntity = entity;
	});
	assert(visitedCount == 1);
	assert(visitedEntity == reusedIndex);

	storage.Remove(reusedIndex);
	assert(!storage.Contains(reusedIndex));
	assert(storage.Size() == 0);
	assert(storage.Capacity() == 4);
}

void TestQueryExclusion(){
	using Query = ECSQuery::ComponentQueryView<
		ECSQuery::Read<TestComponent>
	>;

	const Entity active{1, 1};
	const Entity disabled{2, 1};
	const Entity missingRequired{3, 1};

	Query::AliveSet alive{active, disabled, missingRequired};
	Query::MaskMap masks;
	ComponentMask required;
	ComponentMask excluded;
	constexpr size_t RequiredBit = 3;
	constexpr size_t DisabledBit = 5;
	required.set(RequiredBit);
	excluded.set(DisabledBit);

	masks[active].set(RequiredBit);
	masks[disabled].set(RequiredBit);
	masks[disabled].set(DisabledBit);
	masks[missingRequired].set(DisabledBit);

	Query query(alive, masks, required, excluded);
	std::vector<Entity> result;
	for(Entity entity : query){
		result.push_back(entity);
	}

	assert(result.size() == 1);
	assert(result.front() == active);
}

} // namespace

int main(){
	TestStorageFactory();
	TestDataStorage();
	TestTagStorage();
	TestQueryExclusion();
	return 0;
}
