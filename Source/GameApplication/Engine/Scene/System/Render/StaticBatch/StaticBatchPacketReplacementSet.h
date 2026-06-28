#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "System/Render/RenderSystem/RenderPacket/StaticBatchPacketCache.h"

class StaticBatchPacketReplacementSet {
public:
	void Begin(std::size_t packetCount){
		m_replaced.assign(packetCount, 0);
		m_count = 0;
	}

	void Reset() noexcept {
		m_replaced.clear();
		m_count = 0;
	}

	bool AddGroup(
		const StaticBatchPacketCacheEntry& group,
		std::span<const std::size_t> packetIndices
	){
		if(group.instanceCount == 0 ||
			group.firstInstance > packetIndices.size() ||
			group.instanceCount > packetIndices.size() - group.firstInstance){
			return false;
		}

		for(std::size_t offset = 0; offset < group.instanceCount; ++offset){
			const std::size_t packetIndex =
				packetIndices[group.firstInstance + offset];
			if(packetIndex >= m_replaced.size()) return false;
		}

		for(std::size_t offset = 0; offset < group.instanceCount; ++offset){
			const std::size_t packetIndex =
				packetIndices[group.firstInstance + offset];
			if(m_replaced[packetIndex] == 0){
				m_replaced[packetIndex] = 1;
				++m_count;
			}
		}
		return true;
	}

	bool Contains(std::size_t packetIndex) const noexcept {
		return packetIndex < m_replaced.size() && m_replaced[packetIndex] != 0;
	}

	std::size_t PacketCount() const noexcept { return m_replaced.size(); }
	std::size_t Count() const noexcept { return m_count; }

private:
	std::vector<std::uint8_t> m_replaced;
	std::size_t m_count = 0;
};
