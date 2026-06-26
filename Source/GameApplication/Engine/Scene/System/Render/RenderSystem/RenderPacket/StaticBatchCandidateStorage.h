#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "RenderPacketBuffer.h"

struct StaticBatchCandidateKey {
	RenderPacketKind kind = RenderPacketKind::Model;
	RenderLayer layer = RenderLayer::Opaque3D;
	std::uint32_t materialKey = 0;

	constexpr bool operator==(const StaticBatchCandidateKey&) const noexcept = default;
};

struct StaticBatchCandidate {
	StaticBatchCandidateKey key{};
	std::uint32_t sceneContextID = 0;
	Entity entity{};
	size_t packetIndex = 0;
	std::uint64_t stableSequence = 0;
};

struct StaticBatchCandidateStorageTelemetry {
	size_t currentSize = 0;
	size_t peakSize = 0;
	size_t capacity = 0;
	size_t growthEventCount = 0;
};

class StaticBatchCandidateStorage {
public:
	void BeginFrame(){ m_candidates.clear(); }
	void Reset() noexcept { m_candidates.clear(); }

	void Reserve(size_t count){
		m_candidates.reserve(count);
	}

	void Add(StaticBatchCandidate candidate){
		const size_t capacityBefore = m_candidates.capacity();
		m_candidates.push_back(candidate);
		if(m_candidates.capacity() > capacityBefore){
			++m_growthEventCount;
		}
		m_peakSize = (std::max)(m_peakSize, m_candidates.size());
	}

	void Sort(){
		std::stable_sort(
			m_candidates.begin(),
			m_candidates.end(),
			[](const StaticBatchCandidate& lhs, const StaticBatchCandidate& rhs){
				if(lhs.key.kind != rhs.key.kind){
					return static_cast<std::uint8_t>(lhs.key.kind) <
						static_cast<std::uint8_t>(rhs.key.kind);
				}
				if(lhs.key.layer != rhs.key.layer){
					return static_cast<std::uint8_t>(lhs.key.layer) <
						static_cast<std::uint8_t>(rhs.key.layer);
				}
				if(lhs.key.materialKey != rhs.key.materialKey){
					return lhs.key.materialKey < rhs.key.materialKey;
				}
				if(lhs.sceneContextID != rhs.sceneContextID){
					return lhs.sceneContextID < rhs.sceneContextID;
				}
				return lhs.stableSequence < rhs.stableSequence;
			}
		);
	}

	std::span<const StaticBatchCandidate> Candidates() const noexcept {
		return m_candidates;
	}

	size_t Size() const noexcept { return m_candidates.size(); }
	size_t Capacity() const noexcept { return m_candidates.capacity(); }
	size_t PeakSize() const noexcept { return m_peakSize; }
	size_t GrowthEventCount() const noexcept { return m_growthEventCount; }

	StaticBatchCandidateStorageTelemetry Telemetry() const noexcept {
		return {
			m_candidates.size(),
			m_peakSize,
			m_candidates.capacity(),
			m_growthEventCount
		};
	}

	void ResetPeakMetrics() noexcept {
		m_peakSize = m_candidates.size();
		m_growthEventCount = 0;
	}

private:
	std::vector<StaticBatchCandidate> m_candidates;
	size_t m_peakSize = 0;
	size_t m_growthEventCount = 0;
};

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
