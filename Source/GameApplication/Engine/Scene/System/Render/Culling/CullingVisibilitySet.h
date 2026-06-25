// =======================================================================
//
// CullingVisibilitySet.h
//
// =======================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "Scene/Entity/Entity.h"

struct CullingViewKey {
	std::uint32_t sceneContextID = 0;
	Entity camera{};

	constexpr bool operator==(const CullingViewKey&) const noexcept = default;
};

struct CullingViewKeyHash {
	size_t operator()(const CullingViewKey& key) const noexcept {
		const size_t sceneHash = std::hash<std::uint32_t>{}(key.sceneContextID);
		const size_t cameraHash = std::hash<Entity>{}(key.camera);
		return sceneHash ^ (cameraHash + 0x9e3779b9u + (sceneHash << 6) + (sceneHash >> 2));
	}
};

// Viewごとの一時可視結果。
// CullingComponentへ単一isVisibleを保存せず、Editor / Player / Shadow Viewを分離する。
class CullingVisibilitySet {
public:
	using VisibleEntitySet = std::unordered_set<Entity>;

	void BeginFrame(std::uint64_t frameSerial){
		m_frameSerial = frameSerial;
		m_views.clear();
	}

	void BeginView(CullingViewKey view){
		m_views[view].clear();
	}

	void SetVisible(CullingViewKey view, Entity entity){
		m_views[view].insert(entity);
	}

	bool HasView(CullingViewKey view) const {
		return m_views.contains(view);
	}

	bool IsVisible(CullingViewKey view, Entity entity) const {
		auto iterator = m_views.find(view);
		return iterator != m_views.end() && iterator->second.contains(entity);
	}

	void ClearView(CullingViewKey view){
		m_views.erase(view);
	}

	void ReserveView(CullingViewKey view, size_t expectedVisibleCount){
		m_views[view].reserve(expectedVisibleCount);
	}

	size_t VisibleCount(CullingViewKey view) const noexcept {
		auto iterator = m_views.find(view);
		return iterator != m_views.end() ? iterator->second.size() : 0;
	}

	size_t ViewCount() const noexcept {
		return m_views.size();
	}

	std::uint64_t FrameSerial() const noexcept {
		return m_frameSerial;
	}

private:
	std::uint64_t m_frameSerial = 0;
	std::unordered_map<CullingViewKey, VisibleEntitySet, CullingViewKeyHash> m_views;
};
