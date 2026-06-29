#include <array>
#include <cassert>
#include <cstddef>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchShadowGroupEligibility.h"

namespace {

StaticBatchPacketCacheEntry MakeGroup(){
	StaticBatchPacketCacheEntry group;
	group.key.kind = RenderPacketKind::Model;
	group.key.layer = RenderLayer::Opaque3D;
	group.key.materialKey = 4;
	group.sceneContextID = 7;
	group.representativePacketIndex = 0;
	group.firstInstance = 0;
	group.instanceCount = 2;
	return group;
}

RenderPacket MakePacket(std::uint32_t entityIndex){
	RenderPacket packet;
	packet.sceneContextID = 7;
	packet.entity = Entity(entityIndex, 1);
	packet.kind = RenderPacketKind::Model;
	packet.layer = RenderLayer::Opaque3D;
	packet.passMask =
		RenderPacketPassMask::Shadow | RenderPacketPassMask::GBuffer;
	packet.materialKey = 4;
	packet.subMeshIndex = 0;
	return packet;
}

} // namespace

int main(){
	std::array<RenderPacket, 2> packets{
		MakePacket(1),
		MakePacket(2)
	};
	constexpr std::array<std::size_t, 2> packetIndices{0, 1};

	StaticBatchPacketCacheEntry group = MakeGroup();
	auto result = StaticBatchShadowGroupEligibility::Resolve(
		group,
		packetIndices,
		packets
	);
	assert(
		result.rejectReason ==
		StaticBatchShadowGroupRejectReason::MaterialUnavailable
	);

	group.instanceCount = 1;
	result = StaticBatchShadowGroupEligibility::Resolve(
		group,
		packetIndices,
		packets
	);
	assert(
		result.rejectReason ==
		StaticBatchShadowGroupRejectReason::SingleInstance
	);

	group = MakeGroup();
	group.firstInstance = 1;
	result = StaticBatchShadowGroupEligibility::Resolve(
		group,
		packetIndices,
		packets
	);
	assert(
		result.rejectReason ==
		StaticBatchShadowGroupRejectReason::InvalidPacketRange
	);

	group = MakeGroup();
	packets[1].passMask = RenderPacketPassMask::GBuffer;
	result = StaticBatchShadowGroupEligibility::Resolve(
		group,
		packetIndices,
		packets
	);
	assert(
		result.rejectReason ==
		StaticBatchShadowGroupRejectReason::MissingShadowPass
	);

	packets[1] = MakePacket(2);
	packets[1].materialKey = 9;
	result = StaticBatchShadowGroupEligibility::Resolve(
		group,
		packetIndices,
		packets
	);
	assert(
		result.rejectReason ==
		StaticBatchShadowGroupRejectReason::GroupPacketMismatch
	);

	packets[1] = MakePacket(2);
	packets[0].kind = RenderPacketKind::Mesh;
	result = StaticBatchShadowGroupEligibility::Resolve(
		group,
		packetIndices,
		packets
	);
	assert(
		result.rejectReason ==
		StaticBatchShadowGroupRejectReason::UnsupportedPacketKind
	);

	packets[0] = MakePacket(1);
	packets[0].layer = RenderLayer::Transparent3D;
	result = StaticBatchShadowGroupEligibility::Resolve(
		group,
		packetIndices,
		packets
	);
	assert(
		result.rejectReason ==
		StaticBatchShadowGroupRejectReason::UnsupportedRenderLayer
	);
	return 0;
}
