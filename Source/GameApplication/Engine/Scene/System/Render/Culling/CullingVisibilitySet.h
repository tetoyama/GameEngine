// =======================================================================
//
// CullingVisibilitySet.h
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "Scene/Entity/Entity.h"

enum class CullingViewKind : std::uint8_t {
	Custom,
	Editor,
	Player,
	Shadow
};

struct CullingViewKey {
	std::uint32_t sceneContextID = 0;
	Entity camera{};
	CullingViewKind kind = CullingViewKind::Custom;
	std::uint32_t instanceID = 0;

	constexpr bool operator==(const CullingViewKey&) const noexcept = default;
};

struct CullingViewKeyHash {
	size_t operator()(const CullingViewKey& key) const noexcept {
		size_t hash = std::hash<std::uint32_t>{}(key.sceneContextID);
		const auto combine = [&hash](size_t value){
			hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		};
		combine(std::hash<Entity>{}(key.camera));
		combine(std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(key.kind)));
		combine(std::hash<std::uint32_t>{}(key.instanceID));
		return hash;
	}
};

class CullingVisibilitySet {
public:
	using VisibleEntitySet = std::unordered_set<Entity>;

	void BeginFrame(std::uint64_t frameSerial){
		m_frameSerial = frameSerial;
		++m_activationEpoch;
		if(m_activationEpoch == 0) ++m_activationEpoch;
	}

	void BeginView(CullingViewKey view){
		ViewEntry& entry = m_views[view];
		entry.entities.clear();
		entry.activationEpoch = m_activationEpoch;
	}

	void SetVisible(CullingViewKey view, Entity entity){
		ViewEntry& entry = m_views[view];
		if(entry.activationEpoch != m_activationEpoch){
			entry.entities.clear();
			entry.activationEpoch = m_activationEpoch;
		}

		const size_t bucketsBefore = entry.entities.bucket_count();
		const auto [iterator, inserted] = entry.entities.insert(entity);
		(void)iterator;
		if(!inserted) return;

		entry.peakVisibleCount = (std::max)(entry.peakVisibleCount, entry.entities.size());
		if(entry.entities.bucket_count() > bucketsBefore) ++m_growthEventCount;
	}

	bool HasView(CullingViewKey view) const {
		auto iterator = m_views.find(view);
		return iterator != m_views.end() &&
			iterator->second.activationEpoch == m_activationEpoch;
	}

	bool IsVisible(CullingViewKey view, Entity entity) const {
		auto iterator = m_views.find(view);
		return iterator != m_views.end() &&
			iterator->second.activationEpoch == m_activationEpoch &&
			iterator->second.entities.contains(entity);
	}

	void ClearView(CullingViewKey view){
		m_views.erase(view);
	}

	void ReserveView(CullingViewKey view, size_t expectedVisibleCount){
		ViewEntry& entry = m_views[view];
		if(entry.activationEpoch != m_activationEpoch){
			entry.entities.clear();
			entry.activationEpoch = m_activationEpoch;
		}
		entry.entities.reserve(expectedVisibleCount);
	}

	size_t VisibleCount(CullingViewKey view) const noexcept {
		auto iterator = m_views.find(view);
		return iterator != m_views.end() &&
			iterator->second.activationEpoch == m_activationEpoch
			? iterator->second.entities.size()
			: 0;
	}

	size_t MaxVisibleCount() const noexcept {
		size_t count = 0;
		for(const auto& [key, entry] : m_views){
			(void)key;
			if(entry.activationEpoch == m_activationEpoch){
				count = (std::max)(count, entry.entities.size());
			}
		}
		return count;
	}

	size_t PeakVisibleCount() const noexcept {
		size_t count = 0;
		for(const auto& [key, entry] : m_views){
			(void)key;
			count = (std::max)(count, entry.peakVisibleCount);
		}
		return count;
	}

	size_t GrowthEventCount() const noexcept { return m_growthEventCount; }

	void ResetPeakMetrics() noexcept {
		m_growthEventCount = 0;
		for(auto& [key, entry] : m_views){
			(void)key;
			entry.peakVisibleCount = entry.activationEpoch == m_activationEpoch
				? entry.entities.size()
				: 0;
		}
	}

	size_t ViewCount() const noexcept {
		size_t count = 0;
		for(const auto& [key, entry] : m_views){
			(void)key;
			if(entry.activationEpoch == m_activationEpoch) ++count;
		}
		return count;
	}

	std::uint64_t FrameSerial() const noexcept { return m_frameSerial; }

private:
	struct ViewEntry {
		VisibleEntitySet entities;
		std::uint64_t activationEpoch = 0;
		size_t peakVisibleCount = 0;
	};

	std::uint64_t m_frameSerial = 0;
	std::uint64_t m_activationEpoch = 0;
	size_t m_growthEventCount = 0;
	std::unordered_map<CullingViewKey, ViewEntry, CullingViewKeyHash> m_views;
};
