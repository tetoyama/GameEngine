#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "RenderPacket.h"
#include "StaticBatchCandidateTypes.h"

struct StaticBatchPacketCacheEntry {
	StaticBatchCandidateKey key{};
	std::uint32_t sceneContextID = 0;
	size_t firstInstance = 0;
	size_t instanceCount = 0;
};

struct StaticBatchPacketCacheTelemetry {
	size_t currentEntryCount = 0;
	size_t peakEntryCount = 0;
	size_t entryCapacity = 0;
	size_t currentInstanceCount = 0;
	size_t peakInstanceCount = 0;
	size_t instanceCapacity = 0;
	size_t rebuildCount = 0;
	size_t growthEventCount = 0;
	size_t overflowEventCount = 0;
	size_t skippedIncompleteGroupCount = 0;
	bool valid = false;
	bool overflowed = false;
};

class StaticBatchPacketCache {
public:
	void Reserve(size_t expectedCandidateCount){
		m_entries.reserve(expectedCandidateCount);
		m_entities.reserve(expectedCandidateCount);
		m_transforms.reserve(expectedCandidateCount);
	}

	void Reset() noexcept {
		m_entries.clear();
		m_entities.clear();
		m_transforms.clear();
		m_sourceRevision = 0;
		m_generation = 0;
		m_valid = false;
		m_overflowed = false;
		m_skippedIncompleteGroupCount = 0;
	}

	bool Synchronize(
		const StaticBatchCandidateStorage& candidates,
		std::span<const RenderPacket> packets,
		std::uint64_t generation,
		bool allowRuntimeGrowth
	){
		m_skippedIncompleteGroupCount = 0;
		if(candidates.IsOverflowed()){
			InvalidateForOverflow();
			return false;
		}

		size_t requiredEntryCount = 0;
		size_t requiredInstanceCount = 0;
		for(const StaticBatchCandidateGroup& group : candidates.Groups()){
			if(!group.cacheReady){
				++m_skippedIncompleteGroupCount;
				continue;
			}
			if(!IsGroupRangeValid(group, candidates.Candidates(), packets)){
				++m_skippedIncompleteGroupCount;
				continue;
			}
			++requiredEntryCount;
			requiredInstanceCount += group.candidateCount;
		}

		const std::uint64_t revision = ComputeSourceRevision(
			candidates,
			packets
		);
		if(m_valid && !m_overflowed && revision == m_sourceRevision){
			m_generation = generation;
			return true;
		}

		if(!allowRuntimeGrowth &&
			(requiredEntryCount > m_entries.capacity() ||
			 requiredInstanceCount > m_entities.capacity() ||
			 requiredInstanceCount > m_transforms.capacity())){
			InvalidateForOverflow();
			return false;
		}

		const size_t entryCapacityBefore = m_entries.capacity();
		const size_t entityCapacityBefore = m_entities.capacity();
		const size_t transformCapacityBefore = m_transforms.capacity();
		m_entries.reserve(requiredEntryCount);
		m_entities.reserve(requiredInstanceCount);
		m_transforms.reserve(requiredInstanceCount);
		if(m_entries.capacity() > entryCapacityBefore ||
			m_entities.capacity() > entityCapacityBefore ||
			m_transforms.capacity() > transformCapacityBefore){
			++m_growthEventCount;
		}

		m_entries.clear();
		m_entities.clear();
		m_transforms.clear();

		for(const StaticBatchCandidateGroup& group : candidates.Groups()){
			if(!group.cacheReady ||
				!IsGroupRangeValid(group, candidates.Candidates(), packets)){
				continue;
			}

			const size_t firstInstance = m_entities.size();
			for(size_t offset = 0; offset < group.candidateCount; ++offset){
				const StaticBatchCandidate& candidate =
					candidates.Candidates()[group.firstCandidate + offset];
				const RenderPacket& packet = packets[candidate.packetIndex];
				m_entities.push_back(candidate.entity);
				m_transforms.push_back(packet.transform);
			}

			m_entries.push_back({
				group.key,
				group.sceneContextID,
				firstInstance,
				group.candidateCount
			});
		}

		m_sourceRevision = revision;
		m_generation = generation;
		m_valid = true;
		m_overflowed = false;
		++m_rebuildCount;
		m_peakEntryCount = (std::max)(m_peakEntryCount, m_entries.size());
		m_peakInstanceCount = (std::max)(
			m_peakInstanceCount,
			m_entities.size()
		);
		return true;
	}

	std::span<const StaticBatchPacketCacheEntry> Entries() const noexcept {
		return m_entries;
	}

	std::span<const Entity> Entities() const noexcept {
		return m_entities;
	}

	std::span<const RenderPacketTransformSnapshot> Transforms() const noexcept {
		return m_transforms;
	}

	std::uint64_t Generation() const noexcept { return m_generation; }
	std::uint64_t SourceRevision() const noexcept { return m_sourceRevision; }
	bool IsValid() const noexcept { return m_valid; }
	bool IsOverflowed() const noexcept { return m_overflowed; }

	StaticBatchPacketCacheTelemetry Telemetry() const noexcept {
		return {
			m_entries.size(),
			m_peakEntryCount,
			m_entries.capacity(),
			m_entities.size(),
			m_peakInstanceCount,
			(std::min)(m_entities.capacity(), m_transforms.capacity()),
			m_rebuildCount,
			m_growthEventCount,
			m_overflowEventCount,
			m_skippedIncompleteGroupCount,
			m_valid,
			m_overflowed
		};
	}

	void ResetPeakMetrics() noexcept {
		m_peakEntryCount = m_entries.size();
		m_peakInstanceCount = m_entities.size();
		m_rebuildCount = 0;
		m_growthEventCount = 0;
		m_overflowEventCount = 0;
	}

private:
	static void HashValue(std::uint64_t& hash, std::uint64_t value) noexcept {
		hash ^= value + 0x9e3779b97f4a7c15ull +
			(hash << 6) + (hash >> 2);
	}

	static bool IsGroupRangeValid(
		const StaticBatchCandidateGroup& group,
		std::span<const StaticBatchCandidate> candidates,
		std::span<const RenderPacket> packets
	) noexcept {
		if(group.firstCandidate > candidates.size()) return false;
		if(group.candidateCount > candidates.size() - group.firstCandidate){
			return false;
		}
		for(size_t offset = 0; offset < group.candidateCount; ++offset){
			const StaticBatchCandidate& candidate =
				candidates[group.firstCandidate + offset];
			if(candidate.packetIndex >= packets.size()) return false;
		}
		return true;
	}

	static std::uint64_t ComputeSourceRevision(
		const StaticBatchCandidateStorage& storage,
		std::span<const RenderPacket> packets
	) noexcept {
		std::uint64_t hash = 0x5354415449434341ull;
		for(const StaticBatchCandidateGroup& group : storage.Groups()){
			if(!group.cacheReady ||
				!IsGroupRangeValid(group, storage.Candidates(), packets)){
				continue;
			}

			HashValue(hash, group.sceneContextID);
			HashValue(hash, static_cast<std::uint64_t>(group.key.kind));
			HashValue(hash, static_cast<std::uint64_t>(group.key.layer));
			HashValue(hash, group.key.materialKey);
			HashValue(hash, group.key.pipelineKey);
			HashValue(hash, group.key.geometryKey);
			HashValue(hash, group.key.textureSetKey);
			HashValue(hash, group.candidateCount);

			for(size_t offset = 0; offset < group.candidateCount; ++offset){
				const StaticBatchCandidate& candidate =
					storage.Candidates()[group.firstCandidate + offset];
				const RenderPacket& packet = packets[candidate.packetIndex];
				HashValue(hash, candidate.entity.GetPackedValue());
				for(float value : packet.transform.worldMatrix.values){
					HashValue(hash, std::bit_cast<std::uint32_t>(value));
				}
			}
		}
		return hash == 0 ? 1 : hash;
	}

	void InvalidateForOverflow() noexcept {
		m_entries.clear();
		m_entities.clear();
		m_transforms.clear();
		m_sourceRevision = 0;
		m_valid = false;
		m_overflowed = true;
		++m_overflowEventCount;
	}

	std::vector<StaticBatchPacketCacheEntry> m_entries;
	std::vector<Entity> m_entities;
	std::vector<RenderPacketTransformSnapshot> m_transforms;
	std::uint64_t m_generation = 0;
	std::uint64_t m_sourceRevision = 0;
	size_t m_peakEntryCount = 0;
	size_t m_peakInstanceCount = 0;
	size_t m_rebuildCount = 0;
	size_t m_growthEventCount = 0;
	size_t m_overflowEventCount = 0;
	size_t m_skippedIncompleteGroupCount = 0;
	bool m_valid = false;
	bool m_overflowed = false;
};
