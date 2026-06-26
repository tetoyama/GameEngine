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
		combine(std::hash<std::uint8_t>{}(
			static_cast<std::uint8_t>(key.kind)
		));
		combine(std::hash<std::uint32_t>{}(key.instanceID));
		return hash;
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
