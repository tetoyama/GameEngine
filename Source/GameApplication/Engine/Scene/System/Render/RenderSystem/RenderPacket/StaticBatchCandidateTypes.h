#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "RenderPacket.h"

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
	bool overflowed = false;
};

class StaticBatchCandidateStorage {
public:
	void BeginFrame(){
		m_candidates.clear();
		m_overflowed = false;
	}

	void Reset() noexcept {
		m_candidates.clear();
		m_overflowed = false;
	}

	// Explicit scene configuration is not a runtime growth event.
	void Reserve(size_t count){ m_candidates.reserve(count); }

	void SetRuntimeGrowthAllowed(bool allowed) noexcept {
		m_allowRuntimeGrowth = allowed;
	}

	bool Add(StaticBatchCandidate candidate){
		if(m_overflowed) return false;
		if(!m_allowRuntimeGrowth &&
			m_candidates.size() >= m_candidates.capacity()){
			// A partial candidate list must never be consumed as a complete batch set.
			// Clearing it safely falls back to the ordinary RenderPacket path.
			m_candidates.clear();
			m_overflowed = true;
			++m_growthEventCount;
			return false;
		}

		const size_t capacityBefore = m_candidates.capacity();
		m_candidates.push_back(candidate);
		if(m_candidates.capacity() > capacityBefore){
			++m_growthEventCount;
		}
		m_peakSize = (std::max)(m_peakSize, m_candidates.size());
		return true;
	}

	void Sort(){
		if(m_overflowed) return;
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
	bool IsOverflowed() const noexcept { return m_overflowed; }

	StaticBatchCandidateStorageTelemetry Telemetry() const noexcept {
		return {
			m_candidates.size(),
			m_peakSize,
			m_candidates.capacity(),
			m_growthEventCount,
			m_overflowed
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
	bool m_allowRuntimeGrowth = true;
	bool m_overflowed = false;
};
