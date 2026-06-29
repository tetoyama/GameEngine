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
	std::uint32_t subMeshIndex = RenderPacketAllSubMeshes;
	std::uint64_t pipelineKey = 0;
	std::uint64_t geometryKey = 0;
	std::uint64_t textureSetKey = 0;
	std::uint64_t materialStateKey = 0;

	constexpr bool operator==(const StaticBatchCandidateKey&) const noexcept = default;

	bool IsCacheReady() const noexcept {
		return pipelineKey != 0 &&
			geometryKey != 0 &&
			textureSetKey != 0 &&
			materialStateKey != 0;
	}
};

struct StaticBatchCandidate {
	StaticBatchCandidateKey key{};
	std::uint32_t sceneContextID = 0;
	Entity entity{};
	size_t packetIndex = 0;
	std::uint64_t stableSequence = 0;
};

struct StaticBatchCandidateGroup {
	StaticBatchCandidateKey key{};
	std::uint32_t sceneContextID = 0;
	size_t firstCandidate = 0;
	size_t candidateCount = 0;
	bool cacheReady = false;
};

struct StaticBatchCandidateStorageTelemetry {
	size_t currentSize = 0;
	size_t peakSize = 0;
	size_t capacity = 0;
	size_t growthEventCount = 0;
	size_t currentGroupCount = 0;
	size_t peakGroupCount = 0;
	size_t groupCapacity = 0;
	size_t currentCacheReadyGroupCount = 0;
	size_t peakCacheReadyGroupCount = 0;
	bool overflowed = false;
};

class StaticBatchCandidateStorage {
public:
	void BeginFrame(){
		m_candidates.clear();
		m_groups.clear();
		m_cacheReadyGroupCount = 0;
		m_overflowed = false;
	}

	void Reset() noexcept {
		m_candidates.clear();
		m_groups.clear();
		m_cacheReadyGroupCount = 0;
		m_overflowed = false;
	}

	void Reserve(size_t count){
		m_candidates.reserve(count);
		m_groups.reserve(count);
	}

	void SetRuntimeGrowthAllowed(bool allowed) noexcept {
		m_allowRuntimeGrowth = allowed;
	}

	bool Add(StaticBatchCandidate candidate){
		if(m_overflowed) return false;
		if(!m_allowRuntimeGrowth &&
			m_candidates.size() >= m_candidates.capacity()){
			m_candidates.clear();
			m_groups.clear();
			m_cacheReadyGroupCount = 0;
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
		m_groups.clear();
		m_cacheReadyGroupCount = 0;
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
				if(lhs.key.subMeshIndex != rhs.key.subMeshIndex){
					return lhs.key.subMeshIndex < rhs.key.subMeshIndex;
				}
				if(lhs.key.pipelineKey != rhs.key.pipelineKey){
					return lhs.key.pipelineKey < rhs.key.pipelineKey;
				}
				if(lhs.key.geometryKey != rhs.key.geometryKey){
					return lhs.key.geometryKey < rhs.key.geometryKey;
				}
				if(lhs.key.textureSetKey != rhs.key.textureSetKey){
					return lhs.key.textureSetKey < rhs.key.textureSetKey;
				}
				if(lhs.key.materialStateKey != rhs.key.materialStateKey){
					return lhs.key.materialStateKey < rhs.key.materialStateKey;
				}
				if(lhs.sceneContextID != rhs.sceneContextID){
					return lhs.sceneContextID < rhs.sceneContextID;
				}
				return lhs.stableSequence < rhs.stableSequence;
			}
		);

		BuildGroups();
	}

	std::span<const StaticBatchCandidate> Candidates() const noexcept {
		return m_candidates;
	}

	std::span<const StaticBatchCandidateGroup> Groups() const noexcept {
		return m_groups;
	}

	size_t Size() const noexcept { return m_candidates.size(); }
	size_t Capacity() const noexcept { return m_candidates.capacity(); }
	size_t PeakSize() const noexcept { return m_peakSize; }
	size_t GroupCount() const noexcept { return m_groups.size(); }
	size_t GroupCapacity() const noexcept { return m_groups.capacity(); }
	size_t PeakGroupCount() const noexcept { return m_peakGroupCount; }
	size_t CacheReadyGroupCount() const noexcept { return m_cacheReadyGroupCount; }
	size_t PeakCacheReadyGroupCount() const noexcept {
		return m_peakCacheReadyGroupCount;
	}
	size_t GrowthEventCount() const noexcept { return m_growthEventCount; }
	bool IsOverflowed() const noexcept { return m_overflowed; }

	StaticBatchCandidateStorageTelemetry Telemetry() const noexcept {
		return {
			m_candidates.size(),
			m_peakSize,
			m_candidates.capacity(),
			m_growthEventCount,
			m_groups.size(),
			m_peakGroupCount,
			m_groups.capacity(),
			m_cacheReadyGroupCount,
			m_peakCacheReadyGroupCount,
			m_overflowed
		};
	}

	void ResetPeakMetrics() noexcept {
		m_peakSize = m_candidates.size();
		m_peakGroupCount = m_groups.size();
		m_peakCacheReadyGroupCount = m_cacheReadyGroupCount;
		m_growthEventCount = 0;
	}

private:
	void BuildGroups(){
		size_t first = 0;
		while(first < m_candidates.size()){
			const StaticBatchCandidate& head = m_candidates[first];
			size_t end = first + 1;
			while(end < m_candidates.size() &&
				m_candidates[end].key == head.key &&
				m_candidates[end].sceneContextID == head.sceneContextID){
				++end;
			}

			const bool cacheReady = head.key.IsCacheReady();
			const size_t capacityBefore = m_groups.capacity();
			m_groups.push_back({
				head.key,
				head.sceneContextID,
				first,
				end - first,
				cacheReady
			});
			if(m_groups.capacity() > capacityBefore){
				++m_growthEventCount;
			}
			if(cacheReady) ++m_cacheReadyGroupCount;
			first = end;
		}
		m_peakGroupCount = (std::max)(m_peakGroupCount, m_groups.size());
		m_peakCacheReadyGroupCount = (std::max)(
			m_peakCacheReadyGroupCount,
			m_cacheReadyGroupCount
		);
	}

	std::vector<StaticBatchCandidate> m_candidates;
	std::vector<StaticBatchCandidateGroup> m_groups;
	size_t m_peakSize = 0;
	size_t m_peakGroupCount = 0;
	size_t m_cacheReadyGroupCount = 0;
	size_t m_peakCacheReadyGroupCount = 0;
	size_t m_growthEventCount = 0;
	bool m_allowRuntimeGrowth = true;
	bool m_overflowed = false;
};
