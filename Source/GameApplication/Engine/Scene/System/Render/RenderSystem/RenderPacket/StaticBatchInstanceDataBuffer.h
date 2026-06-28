#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include "StaticBatchPacketCache.h"

struct StaticBatchInstanceData {
	std::array<float, 16> worldMatrix{};
	std::uint32_t entityIndex = 0;
	std::uint32_t entityGeneration = 0;
	std::uint32_t sceneContextID = 0;
	std::uint32_t reserved = 0;
};
static_assert(sizeof(StaticBatchInstanceData) == 80);
static_assert(std::is_trivially_copyable_v<StaticBatchInstanceData>);

using StaticBatchInstanceGroup = StaticBatchPacketCacheEntry;

struct StaticBatchInstanceDataTelemetry {
	size_t currentGroupCount = 0;
	size_t peakGroupCount = 0;
	size_t groupCapacity = 0;
	size_t currentInstanceCount = 0;
	size_t peakInstanceCount = 0;
	size_t instanceCapacity = 0;
	size_t rebuildCount = 0;
	size_t growthEventCount = 0;
	size_t overflowEventCount = 0;
	bool valid = false;
	bool overflowed = false;
};

class StaticBatchInstanceDataBuffer {
public:
	void Reserve(size_t count){
		m_groups.reserve(count);
		m_instances.reserve(count);
	}

	void Reset() noexcept {
		m_groups.clear();
		m_instances.clear();
		m_sourceRevision = 0;
		m_generation = 0;
		m_valid = false;
		m_overflowed = false;
	}

	bool Synchronize(const StaticBatchPacketCache& cache, bool allowRuntimeGrowth){
		if(!cache.IsValid() || cache.IsOverflowed()){
			Invalidate(cache.IsOverflowed());
			return false;
		}
		if(m_valid && !m_overflowed && m_sourceRevision == cache.SourceRevision()){
			m_generation = cache.Generation();
			return true;
		}

		const size_t groupCount = cache.Entries().size();
		const size_t instanceCount = cache.Entities().size();
		if(cache.Transforms().size() != instanceCount){
			Invalidate(false);
			return false;
		}
		if(!allowRuntimeGrowth &&
			(groupCount > m_groups.capacity() || instanceCount > m_instances.capacity())){
			Invalidate(true);
			return false;
		}

		const size_t oldGroupCapacity = m_groups.capacity();
		const size_t oldInstanceCapacity = m_instances.capacity();
		m_groups.reserve(groupCount);
		m_instances.reserve(instanceCount);
		if(m_groups.capacity() > oldGroupCapacity ||
			m_instances.capacity() > oldInstanceCapacity){
			++m_growthEventCount;
		}

		m_groups.clear();
		m_instances.clear();
		for(const StaticBatchPacketCacheEntry& entry : cache.Entries()){
			if(!IsRangeValid(entry, cache)){
				Invalidate(false);
				return false;
			}
			const size_t firstOutputInstance = m_instances.size();
			for(size_t offset = 0; offset < entry.instanceCount; ++offset){
				const size_t sourceIndex = entry.firstInstance + offset;
				const Entity entity = cache.Entities()[sourceIndex];
				StaticBatchInstanceData instance;
				for(size_t element = 0; element < instance.worldMatrix.size(); ++element){
					instance.worldMatrix[element] =
						cache.Transforms()[sourceIndex].worldMatrix.values[element];
				}
				instance.entityIndex = entity.GetIndex();
				instance.entityGeneration = entity.GetGeneration();
				instance.sceneContextID = entry.sceneContextID;
				m_instances.push_back(instance);
			}
			m_groups.push_back({
				entry.key,
				entry.sceneContextID,
				entry.representativePacketIndex,
				firstOutputInstance,
				entry.instanceCount
			});
		}

		m_sourceRevision = cache.SourceRevision();
		m_generation = cache.Generation();
		m_valid = true;
		m_overflowed = false;
		++m_rebuildCount;
		m_peakGroupCount = (std::max)(m_peakGroupCount, m_groups.size());
		m_peakInstanceCount = (std::max)(m_peakInstanceCount, m_instances.size());
		return true;
	}

	std::span<const StaticBatchInstanceGroup> Groups() const noexcept { return m_groups; }
	std::span<const StaticBatchInstanceData> Instances() const noexcept { return m_instances; }
	std::uint64_t Generation() const noexcept { return m_generation; }
	std::uint64_t SourceRevision() const noexcept { return m_sourceRevision; }
	bool IsValid() const noexcept { return m_valid; }
	bool IsOverflowed() const noexcept { return m_overflowed; }

	StaticBatchInstanceDataTelemetry Telemetry() const noexcept {
		return {
			m_groups.size(), m_peakGroupCount, m_groups.capacity(),
			m_instances.size(), m_peakInstanceCount, m_instances.capacity(),
			m_rebuildCount, m_growthEventCount, m_overflowEventCount,
			m_valid, m_overflowed
		};
	}

	void ResetPeakMetrics() noexcept {
		m_peakGroupCount = m_groups.size();
		m_peakInstanceCount = m_instances.size();
		m_rebuildCount = 0;
		m_growthEventCount = 0;
		m_overflowEventCount = 0;
	}

private:
	static bool IsRangeValid(
		const StaticBatchPacketCacheEntry& entry,
		const StaticBatchPacketCache& cache
	) noexcept {
		if(entry.firstInstance > cache.Entities().size()) return false;
		if(entry.instanceCount > cache.Entities().size() - entry.firstInstance){
			return false;
		}
		return entry.firstInstance <= cache.Transforms().size() &&
			entry.instanceCount <= cache.Transforms().size() - entry.firstInstance;
	}

	void Invalidate(bool overflowed) noexcept {
		const bool enteringOverflow = overflowed && !m_overflowed;
		m_groups.clear();
		m_instances.clear();
		m_sourceRevision = 0;
		m_generation = 0;
		m_valid = false;
		m_overflowed = overflowed;
		if(enteringOverflow) ++m_overflowEventCount;
	}

	std::vector<StaticBatchInstanceGroup> m_groups;
	std::vector<StaticBatchInstanceData> m_instances;
	std::uint64_t m_generation = 0;
	std::uint64_t m_sourceRevision = 0;
	size_t m_peakGroupCount = 0;
	size_t m_peakInstanceCount = 0;
	size_t m_rebuildCount = 0;
	size_t m_growthEventCount = 0;
	size_t m_overflowEventCount = 0;
	bool m_valid = false;
	bool m_overflowed = false;
};
