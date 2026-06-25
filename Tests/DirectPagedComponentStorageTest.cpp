#include <cassert>
#include <cstdint>
#include <type_traits>

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

	auto directTag = ECSStorage::CreateComponentStorage<TestTag>(
		ComponentStorageStrategy::DirectPaged
	);
	assert(
		dynamic_cast<ECSStorage::DirectPagedTagStorage<TestTag>*>(
			directTag.get()
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
	storage.PreallocatePages(2);

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

	const Entity reusedIndex{1, 2};
	storage.Add(reusedIndex, TestComponent{40});
	assert(!storage.Contains(first));
	assert(storage.Contains(reusedIndex));
	assert(storage.Get(reusedIndex)->value == 40);
}

void TestTagStorage(){
	ECSStorage::DirectPagedTagStorage<TestTag, 4> storage;
	const Entity first{2, 1};
	const Entity reusedIndex{2, 2};

	storage.Add(first);
	assert(storage.Contains(first));
	assert(storage.GetRaw(first) != nullptr);

	storage.Add(reusedIndex);
	assert(!storage.Contains(first));
	assert(storage.Contains(reusedIndex));
	assert(storage.Size() == 1);

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
}

} // namespace

int main(){
	TestStorageFactory();
	TestDataStorage();
	TestTagStorage();
	return 0;
}
