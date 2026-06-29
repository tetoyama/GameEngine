#pragma once

#include <cstdint>

#include "RenderPacket.h"

struct RenderPacketModelSubMeshRange {
	std::uint32_t firstIndex = 0;
	std::uint32_t count = 0;

	constexpr bool IsValid() const noexcept {
		return count != 0;
	}

	constexpr bool IsSingleSubMesh() const noexcept {
		return count == 1;
	}
};

namespace RenderPacketModelSubMeshSelection {

inline constexpr RenderPacketModelSubMeshRange ResolveRange(
	const RenderPacket& packet,
	std::uint32_t meshCount
) noexcept {
	if(packet.kind != RenderPacketKind::Model || meshCount == 0){
		return {};
	}
	if(packet.TargetsAllSubMeshes()){
		return {0, meshCount};
	}
	if(packet.subMeshIndex >= meshCount){
		return {};
	}
	return {packet.subMeshIndex, 1};
}

inline constexpr bool ResolveSingleIndex(
	const RenderPacket& packet,
	std::uint32_t meshCount,
	std::uint32_t& outIndex
) noexcept {
	const RenderPacketModelSubMeshRange range = ResolveRange(packet, meshCount);
	if(!range.IsSingleSubMesh()){
		outIndex = 0;
		return false;
	}
	outIndex = range.firstIndex;
	return true;
}

} // namespace RenderPacketModelSubMeshSelection
