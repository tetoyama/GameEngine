#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "System/Render/StaticBatch/StaticBatchD3D11GeometryBinding.h"
#include "System/Render/StaticBatch/StaticBatchModelGeometrySourceResolver.h"

struct StaticBatchResolvedGeometryGroup {
	std::size_t groupIndex = 0;
	std::uint64_t geometryResourceKey = 0;
	const StaticBatchD3D11GeometryBinding* binding = nullptr;
};

struct StaticBatchGeometryBindingCacheTelemetry {
	std::size_t currentBindingCount = 0;
	std::size_t peakBindingCount = 0;
	std::size_t currentResolvedGroupCount = 0;
	std::size_t peakResolvedGroupCount = 0;
	std::size_t synchronizeCount = 0;
	std::size_t createCount = 0;
	std::size_t reuseCount = 0;
	std::size_t replacementCount = 0;
	std::size_t rejectedGroupCount = 0;
	std::size_t importFailureCount = 0;
	std::size_t sourceConflictCount = 0;
	std::size_t releaseFailureCount = 0;
};

class StaticBatchGeometryBindingCache {
public:
	StaticBatchGeometryBindingCache() = default;
	StaticBatchGeometryBindingCache(
		const StaticBatchGeometryBindingCache&
	) = delete;
	StaticBatchGeometryBindingCache& operator=(
		const StaticBatchGeometryBindingCache&
	) = delete;

	bool Synchronize(
		RHI::IRHIDevice& device,
		std::span<const StaticBatchPacketCacheEntry> groups,
		std::span<const RenderPacket> packets
	){
		m_resolvedGroups.clear();
		++m_synchronizeCount;
		if(device.GetBackendType() != RHI::BackendType::Direct3D11){
			m_rejectedGroupCount += groups.size();
			return false;
		}

		std::vector<std::uint64_t> activeKeys;
		activeKeys.reserve(groups.size());
		m_resolvedGroups.reserve(groups.size());

		for(std::size_t groupIndex = 0;
			groupIndex < groups.size();
			++groupIndex){
			const StaticBatchPacketCacheEntry& group = groups[groupIndex];
			const StaticBatchModelGeometryResolveResult resolved =
				StaticBatchModelGeometrySourceResolver::Resolve(group, packets);
			if(!resolved.IsEligible()){
				RecordReject(resolved.rejectReason);
				continue;
			}

			const std::uint64_t key = resolved.source.geometryResourceKey;
			auto entryIt = FindEntry(key);
			if(entryIt != m_entries.end()){
				if(entryIt->binding && entryIt->binding->Matches(resolved.source)){
					AddActiveKey(activeKeys, key);
					m_resolvedGroups.push_back({
						groupIndex,
						key,
						entryIt->binding.get()
					});
					++m_reuseCount;
					continue;
				}

				if(Contains(activeKeys, key)){
					++m_sourceConflictCount;
					++m_rejectedGroupCount;
					continue;
				}

				auto replacement =
					std::make_unique<StaticBatchD3D11GeometryBinding>();
				if(!replacement->Create(device, resolved.source)){
					++m_importFailureCount;
					++m_rejectedGroupCount;
					continue;
				}
				if(entryIt->binding && !entryIt->binding->Release(device)){
					replacement->Release(device);
					++m_releaseFailureCount;
					++m_rejectedGroupCount;
					continue;
				}

				entryIt->binding = std::move(replacement);
				AddActiveKey(activeKeys, key);
				m_resolvedGroups.push_back({
					groupIndex,
					key,
					entryIt->binding.get()
				});
				++m_replacementCount;
				continue;
			}

			auto binding =
				std::make_unique<StaticBatchD3D11GeometryBinding>();
			if(!binding->Create(device, resolved.source)){
				++m_importFailureCount;
				++m_rejectedGroupCount;
				continue;
			}

			m_entries.push_back({key, std::move(binding)});
			AddActiveKey(activeKeys, key);
			m_resolvedGroups.push_back({
				groupIndex,
				key,
				m_entries.back().binding.get()
			});
			++m_createCount;
		}

		ReleaseInactive(device, activeKeys);
		m_peakBindingCount = (std::max)(
			m_peakBindingCount,
			m_entries.size()
		);
		m_peakResolvedGroupCount = (std::max)(
			m_peakResolvedGroupCount,
			m_resolvedGroups.size()
		);
		return true;
	}

	bool Release(RHI::IRHIDevice& device) noexcept {
		m_resolvedGroups.clear();
		bool releasedAll = true;
		for(auto entryIt = m_entries.begin(); entryIt != m_entries.end();){
			if(!entryIt->binding || entryIt->binding->Release(device)){
				entryIt = m_entries.erase(entryIt);
			}else{
				++m_releaseFailureCount;
				releasedAll = false;
				++entryIt;
			}
		}
		return releasedAll && m_entries.empty();
	}

	std::span<const StaticBatchResolvedGeometryGroup> ResolvedGroups() const noexcept {
		return m_resolvedGroups;
	}

	const StaticBatchD3D11GeometryBinding* FindByGroupIndex(
		std::size_t groupIndex
	) const noexcept {
		const auto found = std::find_if(
			m_resolvedGroups.begin(),
			m_resolvedGroups.end(),
			[groupIndex](const StaticBatchResolvedGeometryGroup& resolved){
				return resolved.groupIndex == groupIndex;
			}
		);
		return found != m_resolvedGroups.end() ? found->binding : nullptr;
	}

	std::size_t BindingCount() const noexcept { return m_entries.size(); }
	std::size_t ResolvedGroupCount() const noexcept {
		return m_resolvedGroups.size();
	}

	StaticBatchGeometryBindingCacheTelemetry Telemetry() const noexcept {
		return {
			m_entries.size(),
			m_peakBindingCount,
			m_resolvedGroups.size(),
			m_peakResolvedGroupCount,
			m_synchronizeCount,
			m_createCount,
			m_reuseCount,
			m_replacementCount,
			m_rejectedGroupCount,
			m_importFailureCount,
			m_sourceConflictCount,
			m_releaseFailureCount
		};
	}

	void ResetMetrics() noexcept {
		m_peakBindingCount = m_entries.size();
		m_peakResolvedGroupCount = m_resolvedGroups.size();
		m_synchronizeCount = 0;
		m_createCount = 0;
		m_reuseCount = 0;
		m_replacementCount = 0;
		m_rejectedGroupCount = 0;
		m_importFailureCount = 0;
		m_sourceConflictCount = 0;
		m_releaseFailureCount = 0;
		m_rejectReasonCounts.fill(0);
	}

	std::size_t RejectCount(
		StaticBatchModelGeometryRejectReason reason
	) const noexcept {
		const std::size_t index = static_cast<std::size_t>(reason);
		return index < m_rejectReasonCounts.size()
			? m_rejectReasonCounts[index]
			: 0;
	}

private:
	struct Entry {
		std::uint64_t geometryResourceKey = 0;
		std::unique_ptr<StaticBatchD3D11GeometryBinding> binding;
	};

	using EntryIterator = std::vector<Entry>::iterator;

	static constexpr std::size_t RejectReasonCount =
		static_cast<std::size_t>(
			StaticBatchModelGeometryRejectReason::GeometryResourceKeyMismatch
		) + 1;

	EntryIterator FindEntry(std::uint64_t key){
		return std::find_if(
			m_entries.begin(),
			m_entries.end(),
			[key](const Entry& entry){
				return entry.geometryResourceKey == key;
			}
		);
	}

	static bool Contains(
		std::span<const std::uint64_t> keys,
		std::uint64_t key
	) noexcept {
		return std::find(keys.begin(), keys.end(), key) != keys.end();
	}

	static void AddActiveKey(
		std::vector<std::uint64_t>& keys,
		std::uint64_t key
	){
		if(!Contains(keys, key)) keys.push_back(key);
	}

	void RecordReject(StaticBatchModelGeometryRejectReason reason) noexcept {
		++m_rejectedGroupCount;
		const std::size_t index = static_cast<std::size_t>(reason);
		if(index < m_rejectReasonCounts.size()){
			++m_rejectReasonCounts[index];
		}
	}

	void ReleaseInactive(
		RHI::IRHIDevice& device,
		std::span<const std::uint64_t> activeKeys
	){
		for(auto entryIt = m_entries.begin(); entryIt != m_entries.end();){
			if(Contains(activeKeys, entryIt->geometryResourceKey)){
				++entryIt;
				continue;
			}
			if(!entryIt->binding || entryIt->binding->Release(device)){
				entryIt = m_entries.erase(entryIt);
			}else{
				++m_releaseFailureCount;
				++entryIt;
			}
		}
	}

	std::vector<Entry> m_entries;
	std::vector<StaticBatchResolvedGeometryGroup> m_resolvedGroups;
	std::array<std::size_t, RejectReasonCount> m_rejectReasonCounts{};
	std::size_t m_peakBindingCount = 0;
	std::size_t m_peakResolvedGroupCount = 0;
	std::size_t m_synchronizeCount = 0;
	std::size_t m_createCount = 0;
	std::size_t m_reuseCount = 0;
	std::size_t m_replacementCount = 0;
	std::size_t m_rejectedGroupCount = 0;
	std::size_t m_importFailureCount = 0;
	std::size_t m_sourceConflictCount = 0;
	std::size_t m_releaseFailureCount = 0;
};
