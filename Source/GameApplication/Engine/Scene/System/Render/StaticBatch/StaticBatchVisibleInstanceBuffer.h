#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "System/Render/RenderSystem/RenderPacket/StaticBatchInstanceDataBuffer.h"

struct StaticBatchVisibleInstanceBufferTelemetry {
	std::size_t sourceGroupCount = 0;
	std::size_t outputGroupCount = 0;
	std::size_t sourceInstanceCount = 0;
	std::size_t visibleInstanceCount = 0;
	std::size_t culledInstanceCount = 0;
	std::size_t allVisibleGroupCount = 0;
	std::size_t allCulledGroupCount = 0;
	std::size_t mixedGroupCount = 0;
	std::size_t growthEventCount = 0;
	std::size_t overflowEventCount = 0;
	bool valid = false;
	bool overflowed = false;
};

class StaticBatchVisibleInstanceBuffer {
public:
	void Reserve(std::size_t groupCount, std::size_t instanceCount){
		m_groups.reserve(groupCount);
		m_instances.reserve(instanceCount);
		m_packetIndices.reserve(instanceCount);
		m_visibilityMask.reserve(instanceCount);
	}

	void Reset() noexcept {
		m_groups.clear();
		m_instances.clear();
		m_packetIndices.clear();
		m_visibilityMask.clear();
		m_sourceRevision = 0;
		m_viewRevision = 0;
		m_selectionRevision = 0;
		m_valid = false;
		m_overflowed = false;
		ResetFrameTelemetry();
	}

	template<class VisibilityPredicate>
	bool Build(
		std::span<const StaticBatchInstanceGroup> groups,
		std::span<const StaticBatchInstanceData> instances,
		std::span<const std::size_t> packetIndices,
		std::uint64_t sourceRevision,
		std::uint64_t viewRevision,
		bool allowRuntimeGrowth,
		VisibilityPredicate&& isVisible
	){
		ResetFrameTelemetry();
		m_sourceGroupCount = groups.size();
		m_sourceInstanceCount = instances.size();

		if(m_valid && !m_overflowed &&
			m_sourceRevision == sourceRevision &&
			m_viewRevision == viewRevision){
			RefreshTelemetry();
			return true;
		}

		if(packetIndices.size() != instances.size() ||
			!ValidateGroupPartition(groups, instances.size())){
			Invalidate(false);
			return false;
		}

		if(!allowRuntimeGrowth &&
			(groups.size() > m_groups.capacity() ||
			 instances.size() > m_instances.capacity() ||
			 instances.size() > m_packetIndices.capacity() ||
			 instances.size() > m_visibilityMask.capacity())){
			Invalidate(true);
			return false;
		}

		const std::size_t groupCapacityBefore = m_groups.capacity();
		const std::size_t instanceCapacityBefore = m_instances.capacity();
		const std::size_t packetCapacityBefore = m_packetIndices.capacity();
		const std::size_t maskCapacityBefore = m_visibilityMask.capacity();
		m_groups.reserve(groups.size());
		m_instances.reserve(instances.size());
		m_packetIndices.reserve(instances.size());
		m_visibilityMask.reserve(instances.size());
		if(m_groups.capacity() > groupCapacityBefore ||
			m_instances.capacity() > instanceCapacityBefore ||
			m_packetIndices.capacity() > packetCapacityBefore ||
			m_visibilityMask.capacity() > maskCapacityBefore){
			++m_growthEventCount;
		}

		m_groups.clear();
		m_instances.clear();
		m_packetIndices.clear();
		m_visibilityMask.assign(instances.size(), std::uint8_t{0});

		for(std::size_t sourceIndex = 0;
			sourceIndex < instances.size();
			++sourceIndex){
			if(isVisible(instances[sourceIndex], sourceIndex)){
				m_visibilityMask[sourceIndex] = 1;
				++m_visibleInstanceCount;
			}else{
				++m_culledInstanceCount;
			}
		}

		for(const StaticBatchInstanceGroup& sourceGroup : groups){
			std::size_t visibleInGroup = 0;
			std::size_t firstVisibleSourceIndex = 0;
			bool foundFirstVisible = false;
			for(std::size_t offset = 0;
				offset < sourceGroup.instanceCount;
				++offset){
				const std::size_t sourceIndex =
					sourceGroup.firstInstance + offset;
				if(m_visibilityMask[sourceIndex] == 0) continue;
				if(!foundFirstVisible){
					firstVisibleSourceIndex = sourceIndex;
					foundFirstVisible = true;
				}
				++visibleInGroup;
			}

			if(visibleInGroup == 0){
				++m_allCulledGroupCount;
				continue;
			}
			if(visibleInGroup == sourceGroup.instanceCount){
				++m_allVisibleGroupCount;
			}else{
				++m_mixedGroupCount;
			}

			const std::size_t outputFirstInstance = m_instances.size();
			for(std::size_t offset = 0;
				offset < sourceGroup.instanceCount;
				++offset){
				const std::size_t sourceIndex =
					sourceGroup.firstInstance + offset;
				if(m_visibilityMask[sourceIndex] == 0) continue;
				m_instances.push_back(instances[sourceIndex]);
				m_packetIndices.push_back(packetIndices[sourceIndex]);
			}

			StaticBatchInstanceGroup outputGroup = sourceGroup;
			outputGroup.representativePacketIndex =
				packetIndices[firstVisibleSourceIndex];
			outputGroup.firstInstance = outputFirstInstance;
			outputGroup.instanceCount = visibleInGroup;
			m_groups.push_back(outputGroup);
		}

		m_sourceRevision = sourceRevision;
		m_viewRevision = viewRevision;
		m_selectionRevision = CombineRevisions(
			sourceRevision,
			viewRevision,
			m_visibleInstanceCount
		);
		m_valid = true;
		m_overflowed = false;
		RefreshTelemetry();
		return true;
	}

	std::span<const StaticBatchInstanceGroup> Groups() const noexcept {
		return m_groups;
	}

	std::span<const StaticBatchInstanceData> Instances() const noexcept {
		return m_instances;
	}

	std::span<const std::size_t> PacketIndices() const noexcept {
		return m_packetIndices;
	}

	std::uint64_t SourceRevision() const noexcept { return m_sourceRevision; }
	std::uint64_t ViewRevision() const noexcept { return m_viewRevision; }
	std::uint64_t SelectionRevision() const noexcept {
		return m_selectionRevision;
	}
	bool IsValid() const noexcept { return m_valid; }
	bool IsOverflowed() const noexcept { return m_overflowed; }

	StaticBatchVisibleInstanceBufferTelemetry Telemetry() const noexcept {
		return {
			m_sourceGroupCount,
			m_groups.size(),
			m_sourceInstanceCount,
			m_visibleInstanceCount,
			m_culledInstanceCount,
			m_allVisibleGroupCount,
			m_allCulledGroupCount,
			m_mixedGroupCount,
			m_growthEventCount,
			m_overflowEventCount,
			m_valid,
			m_overflowed
		};
	}

private:
	static bool ValidateGroupPartition(
		std::span<const StaticBatchInstanceGroup> groups,
		std::size_t instanceCount
	) noexcept {
		std::size_t expectedFirst = 0;
		for(const StaticBatchInstanceGroup& group : groups){
			if(group.instanceCount == 0 ||
				group.firstInstance != expectedFirst ||
				group.instanceCount > instanceCount - expectedFirst){
				return false;
			}
			expectedFirst += group.instanceCount;
		}
		return expectedFirst == instanceCount;
	}

	static std::uint64_t CombineRevisions(
		std::uint64_t sourceRevision,
		std::uint64_t viewRevision,
		std::size_t visibleCount
	) noexcept {
		std::uint64_t hash = sourceRevision;
		hash ^= viewRevision + 0x9e3779b97f4a7c15ull +
			(hash << 6) + (hash >> 2);
		hash ^= static_cast<std::uint64_t>(visibleCount) +
			0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
		return hash == 0 ? 1 : hash;
	}

	void ResetFrameTelemetry() noexcept {
		m_sourceGroupCount = 0;
		m_sourceInstanceCount = 0;
		m_visibleInstanceCount = 0;
		m_culledInstanceCount = 0;
		m_allVisibleGroupCount = 0;
		m_allCulledGroupCount = 0;
		m_mixedGroupCount = 0;
	}

	void RefreshTelemetry() noexcept {
		m_sourceGroupCount = m_sourceGroupCount != 0
			? m_sourceGroupCount
			: m_groups.size();
		m_sourceInstanceCount = m_sourceInstanceCount != 0
			? m_sourceInstanceCount
			: m_visibleInstanceCount + m_culledInstanceCount;
	}

	void Invalidate(bool overflowed) noexcept {
		const bool enteringOverflow = overflowed && !m_overflowed;
		m_groups.clear();
		m_instances.clear();
		m_packetIndices.clear();
		m_visibilityMask.clear();
		m_sourceRevision = 0;
		m_viewRevision = 0;
		m_selectionRevision = 0;
		m_valid = false;
		m_overflowed = overflowed;
		if(enteringOverflow) ++m_overflowEventCount;
		RefreshTelemetry();
	}

	std::vector<StaticBatchInstanceGroup> m_groups;
	std::vector<StaticBatchInstanceData> m_instances;
	std::vector<std::size_t> m_packetIndices;
	std::vector<std::uint8_t> m_visibilityMask;
	std::uint64_t m_sourceRevision = 0;
	std::uint64_t m_viewRevision = 0;
	std::uint64_t m_selectionRevision = 0;
	std::size_t m_sourceGroupCount = 0;
	std::size_t m_sourceInstanceCount = 0;
	std::size_t m_visibleInstanceCount = 0;
	std::size_t m_culledInstanceCount = 0;
	std::size_t m_allVisibleGroupCount = 0;
	std::size_t m_allCulledGroupCount = 0;
	std::size_t m_mixedGroupCount = 0;
	std::size_t m_growthEventCount = 0;
	std::size_t m_overflowEventCount = 0;
	bool m_valid = false;
	bool m_overflowed = false;
};
