#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "RenderPacketBuffer.h"
#include "StaticBatchCandidateTypes.h"

namespace StaticBatchCandidateBuilder {

inline bool IsEligiblePacket(const RenderPacket& packet) noexcept {
	return packet.kind == RenderPacketKind::Model ||
		packet.kind == RenderPacketKind::Mesh;
}

inline void Build(
	StaticBatchCandidateStorage& output,
	const RenderPacketFrameBuffer& packets
){
	output.BeginFrame();

	size_t configuredReserve = 0;
	std::vector<SceneContext*> reservedScenes;
	for(const RenderPacket& packet : packets.Packets()){
		SceneContext* context = packet.bindings.sceneContext;
		if(!context || !context->component) continue;
		if(std::find(reservedScenes.begin(), reservedScenes.end(), context) ==
			reservedScenes.end()){
			reservedScenes.push_back(context);
			configuredReserve += static_cast<size_t>(
				context->storageConfig.staticBatchReserve
			);
		}
	}
	output.Reserve(configuredReserve);

	for(size_t packetIndex = 0; packetIndex < packets.Packets().size(); ++packetIndex){
		const RenderPacket& packet = packets.Packets()[packetIndex];
		SceneContext* context = packet.bindings.sceneContext;
		if(!context || !context->component) continue;
		if(!IsEligiblePacket(packet)) continue;
		if(!context->component->HasComponent<StaticEntityComponent>(packet.entity)){
			continue;
		}

		output.Add({
			{packet.kind, packet.layer, packet.materialKey},
			packet.sceneContextID,
			packet.entity,
			packetIndex,
			packet.stableSequence
		});
	}
	output.Sort();
}

} // namespace StaticBatchCandidateBuilder
