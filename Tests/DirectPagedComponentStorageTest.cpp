#include <cassert>
#include <cstdint>

#include "Engine/Scene/Storage/DirectPagedComponentStorage.h"

namespace {

struct TestComponent {
	int value = 0;
};

struct TestTag {};

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

	storage.Remove(reusedIndex);
	assert(!storage.Contains(reusedIndex));
	assert(storage.Size() == 0);
}

} // namespace

int main(){
	TestDataStorage();
	TestTagStorage();
	return 0;
}
