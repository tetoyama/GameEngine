#include <cassert>
#include <cstddef>

#include "Engine/Scene/Interface/IComponentStorage.h"
#include "Engine/Scene/Storage/DirectPagedComponentStorage.h"

namespace {

struct TestComponent {
	int value = 0;
};

struct TestTag {};

void TestDenseTelemetry(){
	DenseComponentPool<TestComponent> storage;
	storage.Reserve(4);
	assert(storage.GrowthEventCount() == 0);

	for(std::uint32_t index = 0; index < 4; ++index){
		storage.Add(Entity{index, 1}, TestComponent{static_cast<int>(index)});
	}
	assert(storage.Size() == 4);
	assert(storage.PeakSize() == 4);
	assert(storage.GrowthEventCount() == 0);

	storage.Add(Entity{4, 1}, TestComponent{4});
	assert(storage.Size() == 5);
	assert(storage.PeakSize() == 5);
	assert(storage.GrowthEventCount() > 0);

	storage.Remove(Entity{4, 1});
	assert(storage.Size() == 4);
	assert(storage.PeakSize() == 5);
	storage.ResetPeakMetrics();
	assert(storage.PeakSize() == 4);
	assert(storage.GrowthEventCount() == 0);

	const ComponentStorageTelemetry telemetry = storage.Telemetry();
	assert(telemetry.currentSize == 4);
	assert(telemetry.peakSize == 4);
	assert(telemetry.capacity == storage.Capacity());
	assert(telemetry.growthEventCount == 0);
}

void TestSparseTelemetry(){
	SparseStorage<TestComponent> storage;
	for(std::uint32_t index = 0; index < 64; ++index){
		storage.Add(Entity{index, 1}, TestComponent{static_cast<int>(index)});
	}
	assert(storage.Size() == 64);
	assert(storage.PeakSize() == 64);
	assert(storage.GrowthEventCount() > 0);

	storage.ResetPeakMetrics();
	assert(storage.PeakSize() == 64);
	assert(storage.GrowthEventCount() == 0);

	SparseStorage<TestComponent> reserved;
	reserved.Reserve(128);
	assert(reserved.GrowthEventCount() == 0);
	for(std::uint32_t index = 0; index < 64; ++index){
		reserved.Add(Entity{index, 1}, TestComponent{});
	}
	assert(reserved.GrowthEventCount() == 0);
}

void TestDirectPagedDataTelemetry(){
	ECSStorage::DirectPagedComponentStorage<TestComponent, 4> storage;
	storage.PreallocatePages(1);
	assert(storage.Capacity() == 4);
	assert(storage.GrowthEventCount() == 0);

	storage.Add(Entity{0, 1}, TestComponent{10});
	assert(storage.PeakSize() == 1);
	assert(storage.GrowthEventCount() == 0);

	storage.Add(Entity{4, 1}, TestComponent{20});
	assert(storage.Size() == 2);
	assert(storage.PeakSize() == 2);
	assert(storage.Capacity() == 8);
	assert(storage.GrowthEventCount() == 1);

	storage.Remove(Entity{4, 1});
	assert(storage.PeakSize() == 2);
	storage.ResetPeakMetrics();
	assert(storage.PeakSize() == 1);
	assert(storage.GrowthEventCount() == 0);
}

void TestDirectPagedTagTelemetry(){
	ECSStorage::DirectPagedTagStorage<TestTag, 4> storage;
	storage.PreallocatePages(1);
	storage.Add(Entity{1, 1});
	assert(storage.GrowthEventCount() == 0);

	storage.Add(Entity{8, 1});
	assert(storage.Size() == 2);
	assert(storage.PeakSize() == 2);
	assert(storage.GetAllocatedPageCount() == 2);
	assert(storage.GrowthEventCount() == 1);
}

} // namespace

int main(){
	TestDenseTelemetry();
	TestSparseTelemetry();
	TestDirectPagedDataTelemetry();
	TestDirectPagedTagTelemetry();
	return 0;
}
