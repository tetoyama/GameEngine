// =======================================================================
//
// ComponentStorageFactory.h
//
// =======================================================================
#pragma once

#include <cassert>
#include <memory>

#include "Scene/Interface/IComponentStorage.h"
#include "Scene/Storage/ComponentStorageStrategy.h"
#include "Scene/Storage/DirectPagedComponentStorage.h"

namespace ECSStorage {

namespace Detail {

template<typename T>
void ApplyStoragePreference(
	IComponentStorage& storage,
	ComponentStorageStrategy strategy
){
	using Preference = ComponentStoragePreference<T>;

	constexpr size_t expectedCount = []{
		if constexpr(requires { Preference::ExpectedCount; }){
			return Preference::ExpectedCount;
		}
		return size_t{0};
	}();

	constexpr size_t preallocatedPages = []{
		if constexpr(requires { Preference::PreallocatedPages; }){
			return Preference::PreallocatedPages;
		}
		return size_t{0};
	}();

	if(strategy == ComponentStorageStrategy::DirectPaged){
		if constexpr(IsTagComponentV<T>){
			auto& typed = static_cast<DirectPagedTagStorage<T>&>(storage);
			if(expectedCount > 0){
				typed.ReservePageTable(static_cast<uint32_t>(expectedCount));
			}
			if(preallocatedPages > 0){
				typed.PreallocatePages(static_cast<uint32_t>(preallocatedPages));
			}
		} else {
			auto& typed = static_cast<DirectPagedComponentStorage<T>&>(storage);
			if(expectedCount > 0){
				typed.ReservePageTable(static_cast<uint32_t>(expectedCount));
			}
			if(preallocatedPages > 0){
				typed.PreallocatePages(static_cast<uint32_t>(preallocatedPages));
			}
		}
		return;
	}

	if(expectedCount > 0){
		storage.Reserve(expectedCount);
	}
}

} // namespace Detail

template<typename T>
std::unique_ptr<IComponentStorage> CreateComponentStorage(
	ComponentStorageStrategy strategy
){
	std::unique_ptr<IComponentStorage> storage;

	switch(strategy){
	case ComponentStorageStrategy::Dense:
		storage = std::make_unique<DenseComponentPool<T>>();
		break;

	case ComponentStorageStrategy::DirectPaged:
		if constexpr(IsTagComponentV<T>){
			storage = std::make_unique<DirectPagedTagStorage<T>>();
		} else {
			storage = std::make_unique<DirectPagedComponentStorage<T>>();
		}
		break;

	case ComponentStorageStrategy::SparseStable:
		storage = std::make_unique<SparseStorage<T>>();
		break;

	case ComponentStorageStrategy::Archetype:
		// Archetype Chunk Storage実装までは既存Dense経路へ接続する。
		// 登録契約は先に固定し、将来Factory内部だけを差し替える。
		storage = std::make_unique<DenseComponentPool<T>>();
		break;
	}

	assert(storage && "Unknown ComponentStorageStrategy");
	if(storage){
		Detail::ApplyStoragePreference<T>(*storage, strategy);
	}
	return storage;
}

} // namespace ECSStorage
