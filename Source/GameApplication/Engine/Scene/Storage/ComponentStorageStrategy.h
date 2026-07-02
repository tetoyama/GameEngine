// =======================================================================
//
// ComponentStorageStrategy.h
//
// =======================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ECSStorage {

enum class ComponentStorageStrategy : std::uint8_t {
	Dense,
	DirectPaged,
	SparseStable,
	Archetype
};

// 型ごとのStorage指定と既定初期確保Hint。
// Scene設定が適用される場合は、Registry側からこの値を上書きする。
template<typename T>
struct ComponentStoragePreference {
	static constexpr bool HasExplicitStrategy = false;
	static constexpr ComponentStorageStrategy Strategy =
		ComponentStorageStrategy::SparseStable;
	static constexpr size_t ExpectedCount = 0;
	static constexpr size_t PreallocatedPages = 0;
};

// Tag Componentは特殊化で宣言する。
template<typename T>
struct IsTagComponent: std::false_type {};

template<typename T>
inline constexpr bool IsTagComponentV = IsTagComponent<T>::value;

// Inspectorの通常Component一覧ではなくEntity共通ヘッダーで操作する型。
template<typename T>
struct IsEntityHeaderComponent: std::false_type {};

template<typename T>
inline constexpr bool IsEntityHeaderComponentV =
	IsEntityHeaderComponent<T>::value;

// 通常のECS Queryから自動除外する状態Component。
template<typename T>
struct ExcludeFromDefaultQueries: std::false_type {};

template<typename T>
inline constexpr bool ExcludeFromDefaultQueriesV =
	ExcludeFromDefaultQueries<T>::value;

} // namespace ECSStorage
