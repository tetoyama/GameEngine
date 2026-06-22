#pragma once

#include <algorithm>
#include <cstdint>

#include "Scene/Entity/Entity.h"
#include "System/Render/RenderSystem/renderLayer.h"

enum class RenderPacketKind : uint8_t {
	Model = 0,
	Mesh,
	Sprite,
	Billboard,
	Particle,
	Terrain,
	Wave,
	Effect
};

enum class RenderPacketPassMask : uint8_t {
	None = 0,
	Shadow = 1u << 0,
	GBuffer = 1u << 1,
	Forward = 1u << 2,
	Overlay = 1u << 3,
	Debug = 1u << 4
};

constexpr RenderPacketPassMask operator|(
	RenderPacketPassMask lhs,
	RenderPacketPassMask rhs
) noexcept {
	return static_cast<RenderPacketPassMask>(
		static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs)
	);
}

constexpr RenderPacketPassMask operator&(
	RenderPacketPassMask lhs,
	RenderPacketPassMask rhs
) noexcept {
	return static_cast<RenderPacketPassMask>(
		static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs)
	);
}

constexpr bool HasRenderPacketPass(
	RenderPacketPassMask value,
	RenderPacketPassMask pass
) noexcept {
	return (static_cast<uint8_t>(value) & static_cast<uint8_t>(pass)) != 0;
}

constexpr RenderPacketPassMask RemoveRenderPacketPass(
	RenderPacketPassMask value,
	RenderPacketPassMask pass
) noexcept {
	return static_cast<RenderPacketPassMask>(
		static_cast<uint8_t>(value) & ~static_cast<uint8_t>(pass)
	);
}

constexpr RenderPacketPassMask RenderPacketPassesForLayer(
	RenderLayer layer
) noexcept {
	switch(layer){
		case RenderLayer::Opaque3D:
		case RenderLayer::Background2D:
			return RenderPacketPassMask::Shadow | RenderPacketPassMask::GBuffer;
		case RenderLayer::Transparent3D:
		case RenderLayer::SortTransparent3D:
			return RenderPacketPassMask::Shadow | RenderPacketPassMask::Forward;
		case RenderLayer::OverlayUI:
			return RenderPacketPassMask::Shadow | RenderPacketPassMask::Overlay;
		case RenderLayer::Debug:
			return RenderPacketPassMask::Shadow |
				RenderPacketPassMask::GBuffer |
				RenderPacketPassMask::Debug;
		default:
			return RenderPacketPassMask::None;
	}
}

constexpr RenderPacketPassMask RenderPacketPassesForKind(
	RenderPacketKind kind
) noexcept {
	switch(kind){
		case RenderPacketKind::Model:
		case RenderPacketKind::Billboard:
		case RenderPacketKind::Terrain:
			return RenderPacketPassMask::Shadow |
				RenderPacketPassMask::GBuffer |
				RenderPacketPassMask::Forward |
				RenderPacketPassMask::Debug;
		case RenderPacketKind::Wave:
			return RenderPacketPassMask::GBuffer |
				RenderPacketPassMask::Forward |
				RenderPacketPassMask::Debug;
		case RenderPacketKind::Mesh:
		case RenderPacketKind::Particle:
		case RenderPacketKind::Effect:
			return RenderPacketPassMask::Forward;
		case RenderPacketKind::Sprite:
			return RenderPacketPassMask::Forward |
				RenderPacketPassMask::Overlay;
		default:
			return RenderPacketPassMask::None;
	}
}

constexpr RenderPacketPassMask ResolveRenderPacketPasses(
	RenderLayer layer,
	RenderPacketKind kind
) noexcept {
	return RenderPacketPassesForLayer(layer) & RenderPacketPassesForKind(kind);
}

struct RenderPacketTransformSnapshot {
	float position[3] = {0.0f, 0.0f, 0.0f};
	float worldPosition[3] = {0.0f, 0.0f, 0.0f};
	float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	float scale[3] = {1.0f, 1.0f, 1.0f};
};

constexpr uint16_t EncodeRenderPacketOrder(int32_t order) noexcept {
	const int32_t clamped = (std::max)(-32768, (std::min)(32767, order));
	return static_cast<uint16_t>(clamped + 32768);
}

// Primary sort key layout:
// [63:60] RenderLayer, [59:56] PacketKind, [55:40] signed OrderInLayer,
// [39:08] Material key, [07:00] reserved.
// Scene / Entity / generation are deterministic tie breakers and are intentionally
// kept outside this key so the material grouping contract remains visible.
constexpr uint64_t MakeRenderPacketSortKey(
	RenderLayer layer,
	RenderPacketKind kind,
	uint32_t materialKey,
	int32_t orderInLayer
) noexcept {
	return
		(static_cast<uint64_t>(static_cast<uint8_t>(layer) & 0x0fu) << 60) |
		(static_cast<uint64_t>(static_cast<uint8_t>(kind) & 0x0fu) << 56) |
		(static_cast<uint64_t>(EncodeRenderPacketOrder(orderInLayer)) << 40) |
		(static_cast<uint64_t>(materialKey) << 8);
}

struct RenderPacket {
	uint32_t sceneContextID = 0;
	Entity entity{};
	RenderPacketKind kind = RenderPacketKind::Model;
	RenderLayer layer = RenderLayer::Opaque3D;
	RenderPacketPassMask passMask = RenderPacketPassMask::None;
	uint32_t materialKey = 0;
	int32_t orderInLayer = 0;
	uint64_t sortKey = 0;
	uint64_t stableSequence = 0;
	RenderPacketTransformSnapshot transform;
};

inline bool RenderPacketLess(const RenderPacket& lhs, const RenderPacket& rhs) noexcept {
	if(lhs.sortKey != rhs.sortKey) return lhs.sortKey < rhs.sortKey;
	if(lhs.sceneContextID != rhs.sceneContextID) return lhs.sceneContextID < rhs.sceneContextID;
	if(lhs.entity.GetPackedValue() != rhs.entity.GetPackedValue()){
		return lhs.entity.GetPackedValue() < rhs.entity.GetPackedValue();
	}
	if(lhs.kind != rhs.kind){
		return static_cast<uint8_t>(lhs.kind) < static_cast<uint8_t>(rhs.kind);
	}
	return lhs.stableSequence < rhs.stableSequence;
}


struct RenderPacketViewItem {
	const RenderPacket* packet = nullptr;
	float distanceSq = 0.0f;
};

inline bool RenderPacketBackToFront(
	const RenderPacketViewItem& lhs,
	const RenderPacketViewItem& rhs
) noexcept {
	if(lhs.distanceSq != rhs.distanceSq) return lhs.distanceSq > rhs.distanceSq;
	if(!lhs.packet) return false;
	if(!rhs.packet) return true;
	return RenderPacketLess(*lhs.packet, *rhs.packet);
}

inline bool RenderPacketOverlayOrder(
	const RenderPacket* lhs,
	const RenderPacket* rhs
) noexcept {
	if(!lhs) return false;
	if(!rhs) return true;
	if(lhs->orderInLayer != rhs->orderInLayer){
		return lhs->orderInLayer > rhs->orderInLayer;
	}
	return RenderPacketLess(*lhs, *rhs);
}
