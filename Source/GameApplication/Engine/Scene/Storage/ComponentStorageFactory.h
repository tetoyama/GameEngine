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

template<typename T>
std::unique_ptr<IComponentStorage> CreateComponentStorage(
	ComponentStorageStrategy strategy
){
	switch(strategy){
	case ComponentStorageStrategy::Dense:
		return std::make_unique<DenseComponentPool<T>>();

	case ComponentStorageStrategy::DirectPaged:
		if constexpr(IsTagComponentV<T>){
			return std::make_unique<DirectPagedTagStorage<T>>();
		} else {
			return std::make_unique<DirectPagedComponentStorage<T>>();
		}

	case ComponentStorageStrategy::SparseStable:
		return std::make_unique<SparseStorage<T>>();

	case ComponentStorageStrategy::Archetype:
		// Archetype Chunk Storage実装までは既存Dense経路へ接続する。
		// 登録契約は先に固定し、将来Factory内部だけを差し替える。
		return std::make_unique<DenseComponentPool<T>>();
	}

	assert(false && "Unknown ComponentStorageStrategy");
	return nullptr;
}

} // namespace ECSStorage
