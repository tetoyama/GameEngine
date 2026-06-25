// =======================================================================
//
// ComponentStorageStrategy.h
//
// =======================================================================
#pragma once

#include <cstdint>
#include <type_traits>

namespace ECSStorage {

enum class ComponentStorageStrategy : std::uint8_t {
	Dense,
	DirectPaged,
	SparseStable,
	Archetype
};

// 型ごとの明示Storage指定。
// 旧bool登録Adapterを通る間も、Transformなどの例外を型側から上書きできる。
template<typename T>
struct ComponentStoragePreference {
	static constexpr bool HasExplicitStrategy = false;
	static constexpr ComponentStorageStrategy Strategy =
		ComponentStorageStrategy::SparseStable;
};

// Tag Componentは特殊化で宣言する。
// DirectPaged選択時、falseなら実データStorage、trueならTag Storageを生成する。
template<typename T>
struct IsTagComponent: std::false_type {};

template<typename T>
inline constexpr bool IsTagComponentV = IsTagComponent<T>::value;

} // namespace ECSStorage
