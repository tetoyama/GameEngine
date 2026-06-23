#include <array>
#include <cassert>
#include <vector>

#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"

namespace {

RenderPacket MakePacket(
	uint32_t sceneID,
	Entity entity,
	RenderPacketKind kind,
	RenderLayer layer,
	uint32_t material,
	int32_t order,
	uint64_t sequence
){
	RenderPacket packet;
	packet.sceneContextID = sceneID;
	packet.entity = entity;
	packet.kind = kind;
	packet.layer = layer;
	packet.passMask = RenderPacketPassesForLayer(layer);
	packet.materialKey = material;
	packet.orderInLayer = order;
	packet.sortKey = MakeRenderPacketSortKey(layer, kind, material, order);
	packet.stableSequence = sequence;
	return packet;
}

std::vector<uint64_t> PacketIDs(const RenderPacketFrameBuffer& frame){
	std::vector<uint64_t> values;
	for(const RenderPacket& packet : frame.Packets()){
		values.push_back(
			(static_cast<uint64_t>(packet.sceneContextID) << 48) ^
			packet.entity.GetPackedValue() ^
			(static_cast<uint64_t>(packet.kind) << 40)
		);
	}
	return values;
}

} // namespace

int main(){
	static_assert(!HasRenderPacketPass(
		RenderPacketPassesForLayer(RenderLayer::OverlayUI),
		RenderPacketPassMask::GBuffer
	));
	static_assert(HasRenderPacketPass(
		RenderPacketPassesForLayer(RenderLayer::Opaque3D),
		RenderPacketPassMask::Shadow
	));
	static_assert(HasRenderPacketPass(
		RenderPacketPassesForLayer(RenderLayer::Opaque3D),
		RenderPacketPassMask::GBuffer
	));
	static_assert(HasRenderPacketPass(
		ResolveRenderPacketPasses(
			RenderLayer::Transparent3D,
			RenderPacketKind::Model
		),
		RenderPacketPassMask::Shadow
	));
	static_assert(!HasRenderPacketPass(
		ResolveRenderPacketPasses(
			RenderLayer::Opaque3D,
			RenderPacketKind::Mesh
		),
		RenderPacketPassMask::GBuffer
	));
	static_assert(HasRenderPacketPass(
		ResolveRenderPacketPasses(
			RenderLayer::Debug,
			RenderPacketKind::Model
		),
		RenderPacketPassMask::GBuffer
	));
	static_assert(!HasRenderPacketPass(
		RemoveRenderPacketPass(
			RenderPacketPassMask::Shadow | RenderPacketPassMask::GBuffer,
			RenderPacketPassMask::Shadow
		),
		RenderPacketPassMask::Shadow
	));

	// Forward / Overlay専用Renderableが既定Opaque Layerで消失しないこと。
	static_assert(
		ResolveRenderPacketLayer(RenderLayer::Opaque3D, RenderPacketKind::Sprite) ==
		RenderLayer::OverlayUI
	);
	static_assert(HasRenderPacketPass(
		ResolveRenderPacketPasses(RenderLayer::Opaque3D, RenderPacketKind::Sprite),
		RenderPacketPassMask::Overlay
	));
	static_assert(
		ResolveRenderPacketLayer(RenderLayer::Opaque3D, RenderPacketKind::Particle) ==
		RenderLayer::Transparent3D
	);
	static_assert(HasRenderPacketPass(
		ResolveRenderPacketPasses(RenderLayer::Opaque3D, RenderPacketKind::Particle),
		RenderPacketPassMask::Forward
	));
	static_assert(
		ResolveRenderPacketLayer(RenderLayer::Opaque3D, RenderPacketKind::Effect) ==
		RenderLayer::Transparent3D
	);
	static_assert(HasRenderPacketPass(
		ResolveRenderPacketPasses(RenderLayer::Opaque3D, RenderPacketKind::Effect),
		RenderPacketPassMask::Forward
	));

	RenderPacketWorkerBuffer worker0(0);
	RenderPacketWorkerBuffer worker1(1);

	worker1.Add(MakePacket(2, Entity(7, 1), RenderPacketKind::Wave,
		RenderLayer::Transparent3D, 5, 0, 0));
	worker0.Add(MakePacket(1, Entity(9, 0), RenderPacketKind::Model,
		RenderLayer::Opaque3D, 2, 0, 0));
	worker0.Add(MakePacket(1, Entity(3, 0), RenderPacketKind::Model,
		RenderLayer::Opaque3D, 1, 0, 1));
	worker1.Add(MakePacket(1, Entity(4, 0), RenderPacketKind::Sprite,
		RenderLayer::OverlayUI, 0, -5, 1));

	std::array<RenderPacketWorkerBuffer, 2> reversedWorkers{worker1, worker0};
	RenderPacketFrameBuffer frameA;
	frameA.BeginFrame(11);
	frameA.Merge(reversedWorkers);

	std::array<RenderPacketWorkerBuffer, 2> orderedWorkers{worker0, worker1};
	RenderPacketFrameBuffer frameB;
	frameB.BeginFrame(11);
	frameB.Merge(orderedWorkers);

	assert(frameA.IsReady());
	assert(frameA.Generation() == 11);
	assert(frameA.Size() == 4);
	assert(frameA.Count(RenderPacketKind::Model) == 2);
	assert(PacketIDs(frameA) == PacketIDs(frameB));

	const auto& packets = frameA.Packets();
	for(size_t index = 1; index < packets.size(); ++index){
		assert(!RenderPacketLess(packets[index], packets[index - 1]));
	}
	assert(packets[0].entity == Entity(3, 0));
	assert(packets[1].entity == Entity(9, 0));
	assert(packets[2].kind == RenderPacketKind::Wave);
	assert(packets[3].kind == RenderPacketKind::Sprite);

	RenderPacket nearPacket = MakePacket(
		1, Entity(1, 0), RenderPacketKind::Model,
		RenderLayer::SortTransparent3D, 0, 0, 0
	);
	RenderPacket farPacket = MakePacket(
		1, Entity(2, 0), RenderPacketKind::Model,
		RenderLayer::SortTransparent3D, 0, 0, 1
	);
	std::vector<RenderPacketViewItem> transparentView{
		{&nearPacket, 1.0f},
		{&farPacket, 9.0f}
	};
	std::sort(transparentView.begin(), transparentView.end(), RenderPacketBackToFront);
	assert(transparentView.front().packet == &farPacket);

	RenderPacket lowOrder = MakePacket(
		1, Entity(3, 0), RenderPacketKind::Sprite,
		RenderLayer::OverlayUI, 0, -2, 0
	);
	RenderPacket highOrder = MakePacket(
		1, Entity(4, 0), RenderPacketKind::Sprite,
		RenderLayer::OverlayUI, 0, 4, 1
	);
	std::vector<const RenderPacket*> overlayView{&lowOrder, &highOrder};
	std::sort(overlayView.begin(), overlayView.end(), RenderPacketOverlayOrder);
	assert(overlayView.front() == &highOrder);
	return 0;
}
