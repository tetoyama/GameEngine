// =======================================================================
// 
// Entity.h
// 
// =======================================================================
#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>

// 実行時Entityハンドル。
// indexはComponent Storageや保存データ上の識別子、generationは再利用検出に使う。
struct Entity {
	uint32_t index = 0;
	uint32_t generation = 0;

	constexpr Entity() noexcept = default;
	constexpr Entity(uint32_t indexValue) noexcept
		: index(indexValue), generation(0) {}
	constexpr Entity(uint32_t indexValue, uint32_t generationValue) noexcept
		: index(indexValue), generation(generationValue) {}

	constexpr uint32_t GetIndex() const noexcept { return index; }
	constexpr uint32_t GetGeneration() const noexcept { return generation; }

	constexpr uint64_t GetPackedValue() const noexcept {
		return (static_cast<uint64_t>(generation) << 32) | index;
	}

	constexpr explicit operator bool() const noexcept {
		return index != 0;
	}

	// 既存の保存形式、GPU ObjectID、UI表示との移行互換用。
	// Entity同士の識別には必ずindexとgenerationの両方が使われる。
	constexpr operator uint32_t() const noexcept {
		return index;
	}

	constexpr bool operator==(const Entity&) const noexcept = default;
	constexpr auto operator<=>(const Entity&) const noexcept = default;

	template<std::integral T>
	friend constexpr bool operator==(Entity lhs, T rhs) noexcept {
		return lhs.index == static_cast<uint32_t>(rhs);
	}

	template<std::integral T>
	friend constexpr bool operator==(T lhs, Entity rhs) noexcept {
		return rhs == lhs;
	}

	template<std::integral T>
	friend constexpr auto operator<=>(Entity lhs, T rhs) noexcept {
		return lhs.index <=> static_cast<uint32_t>(rhs);
	}

	template<std::integral T>
	friend constexpr auto operator<=>(T lhs, Entity rhs) noexcept {
		return static_cast<uint32_t>(lhs) <=> rhs.index;
	}
};

namespace std {
	template<>
	struct hash<Entity> {
		size_t operator()(const Entity entity) const noexcept {
			return hash<uint64_t>{}(entity.GetPackedValue());
		}
	};
}
