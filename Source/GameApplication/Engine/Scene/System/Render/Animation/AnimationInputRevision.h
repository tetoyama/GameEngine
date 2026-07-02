#pragma once

#include <bit>
#include <cstdint>
#include <span>
#include <string_view>

#include "Resources/Data/modelData.h"

namespace AnimationInputRevision {

inline void HashByte(std::uint64_t& hash, std::uint8_t value) noexcept {
	hash ^= value;
	hash *= 1099511628211ull;
}

inline void HashUint32(std::uint64_t& hash, std::uint32_t value) noexcept {
	for(unsigned shift = 0; shift < 32; shift += 8){
		HashByte(hash, static_cast<std::uint8_t>(value >> shift));
	}
}

inline void HashString(
	std::uint64_t& hash,
	std::string_view value
) noexcept {
	HashUint32(hash, static_cast<std::uint32_t>(value.size()));
	for(const unsigned char character : value){
		HashByte(hash, character);
	}
}

inline std::uint64_t Compute(
	std::span<const AnimationBlend> animations,
	float animationTime
) noexcept {
	std::uint64_t hash = 14695981039346656037ull;
	HashUint32(hash, static_cast<std::uint32_t>(animations.size()));
	for(const AnimationBlend& animation : animations){
		HashString(hash, animation.name);
		HashUint32(hash, std::bit_cast<std::uint32_t>(animation.weight));
		HashUint32(
			hash,
			std::bit_cast<std::uint32_t>(animation.animationStartTime)
		);
		HashByte(hash, animation.isLoop ? 1u : 0u);
	}
	HashUint32(hash, std::bit_cast<std::uint32_t>(animationTime));
	return hash == 0 ? 1 : hash;
}

} // namespace AnimationInputRevision
