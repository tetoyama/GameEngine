#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>


enum class GpuFrameTimingStatus : std::uint8_t {
	Pending = 0,
	Resolved,
	Invalid,
	Dropped
};

// GPU Timestamp Queryで個別計測する描画区間。
// Player / Editorを分離し、同一Frame内で同名Passが複数回実行されても
// Query結果を混同しない契約とする。
enum class GpuPassTimingScope : std::uint8_t {
	PlayerGBuffer = 0,
	PlayerShadow,
	PlayerLighting,
	PlayerForward,
	PlayerPostEffect,
	PlayerOverlay,
	EditorGBuffer,
	EditorShadow,
	EditorLighting,
	EditorForward,
	EditorPostEffect,
	EditorOverlay,
	EditorPhysicsDebug,
	ImGui,
	Count
};

inline constexpr std::size_t GpuPassTimingScopeCount =
	static_cast<std::size_t>(GpuPassTimingScope::Count);
static_assert(GpuPassTimingScopeCount <= 64);

inline constexpr std::uint64_t GpuPassTimingBit(
	GpuPassTimingScope scope
) noexcept {
	return 1ull << static_cast<std::uint8_t>(scope);
}

inline constexpr const char* GpuPassTimingScopeName(
	GpuPassTimingScope scope
) noexcept {
	switch(scope){
		case GpuPassTimingScope::PlayerGBuffer: return "Player GBuffer";
		case GpuPassTimingScope::PlayerShadow: return "Player Shadow";
		case GpuPassTimingScope::PlayerLighting: return "Player Lighting";
		case GpuPassTimingScope::PlayerForward: return "Player Forward";
		case GpuPassTimingScope::PlayerPostEffect: return "Player Post Effect";
		case GpuPassTimingScope::PlayerOverlay: return "Player Overlay";
		case GpuPassTimingScope::EditorGBuffer: return "Editor GBuffer";
		case GpuPassTimingScope::EditorShadow: return "Editor Shadow";
		case GpuPassTimingScope::EditorLighting: return "Editor Lighting";
		case GpuPassTimingScope::EditorForward: return "Editor Forward";
		case GpuPassTimingScope::EditorPostEffect: return "Editor Post Effect";
		case GpuPassTimingScope::EditorOverlay: return "Editor Overlay";
		case GpuPassTimingScope::EditorPhysicsDebug: return "Editor PhysX Debug";
		case GpuPassTimingScope::ImGui: return "ImGui";
		case GpuPassTimingScope::Count: break;
	}
	return "Unknown GPU Pass";
}

struct GpuFrameTimingResult {
	std::uint64_t frameSerial = 0;
	double seconds = 0.0;
	GpuFrameTimingStatus status = GpuFrameTimingStatus::Invalid;
	std::array<double, GpuPassTimingScopeCount> passSeconds{};
	std::uint64_t resolvedPassMask = 0;
	std::uint64_t invalidPassMask = 0;

	bool IsPassResolved(GpuPassTimingScope scope) const noexcept {
		return (resolvedPassMask & GpuPassTimingBit(scope)) != 0;
	}

	bool IsPassInvalid(GpuPassTimingScope scope) const noexcept {
		return (invalidPassMask & GpuPassTimingBit(scope)) != 0;
	}

	double GetPassSeconds(GpuPassTimingScope scope) const noexcept {
		const std::size_t index = static_cast<std::size_t>(scope);
		return index < passSeconds.size() ? passSeconds[index] : 0.0;
	}
};
