// =======================================================================
//
// RHIHandle.h
//
// Backend非依存の世代付きGPU Resource Handle。
// indexの再利用時もgenerationで古いHandleを拒否する。
//
// =======================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <limits>

namespace RHI {

inline constexpr uint32_t InvalidHandleIndex =
	(std::numeric_limits<uint32_t>::max)();

template<typename Tag>
struct Handle {
	uint32_t index = InvalidHandleIndex;
	uint32_t generation = 0;

	constexpr bool IsValid() const noexcept {
		return index != InvalidHandleIndex && generation != 0;
	}

	constexpr explicit operator bool() const noexcept {
		return IsValid();
	}

	constexpr uint64_t PackedValue() const noexcept {
		return (static_cast<uint64_t>(generation) << 32u) |
			static_cast<uint64_t>(index);
	}

	friend constexpr bool operator==(
		const Handle& lhs,
		const Handle& rhs
	) noexcept = default;
};

struct BufferTag {};
struct TextureTag {};
struct ShaderTag {};
struct PipelineStateTag {};

using BufferHandle = Handle<BufferTag>;
using TextureHandle = Handle<TextureTag>;
using ShaderHandle = Handle<ShaderTag>;
using PipelineStateHandle = Handle<PipelineStateTag>;

} // namespace RHI

namespace std {

template<typename Tag>
struct hash<RHI::Handle<Tag>> {
	size_t operator()(const RHI::Handle<Tag>& handle) const noexcept {
		return std::hash<uint64_t>{}(handle.PackedValue());
	}
};

} // namespace std
