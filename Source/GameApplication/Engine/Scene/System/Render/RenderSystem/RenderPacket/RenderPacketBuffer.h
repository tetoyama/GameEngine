#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "RenderPacket.h"

class RenderPacketWorkerBuffer {
public:
	explicit RenderPacketWorkerBuffer(uint32_t workerIndex = 0) noexcept
		: m_workerIndex(workerIndex) {
	}

	uint32_t WorkerIndex() const noexcept { return m_workerIndex; }

	void Clear(){ m_packets.clear(); }

	void Reserve(size_t count){ m_packets.reserve(count); }

	void Add(RenderPacket packet){
		m_packets.emplace_back(std::move(packet));
	}

	const std::vector<RenderPacket>& Packets() const noexcept { return m_packets; }

private:
	uint32_t m_workerIndex = 0;
	std::vector<RenderPacket> m_packets;
};

// Frame-local CPU packet storage. Scheduler resource Write/Read hazards guarantee
// that Merge completes before MainThread submission reads the published packets.
class RenderPacketFrameBuffer {
public:
	void BeginFrame(uint64_t generation){
		m_generation = generation;
		m_packets.clear();
		m_ready = false;
	}

	// Scene restore / shutdown boundaries invalidate every non-owning binding.
	// Capacity is retained to avoid a new allocation spike after Play/Stop.
	void Reset() noexcept {
		m_packets.clear();
		m_ready = false;
	}

	void Merge(std::span<const RenderPacketWorkerBuffer> workerBuffers){
		std::vector<const RenderPacketWorkerBuffer*> orderedWorkers;
		orderedWorkers.reserve(workerBuffers.size());
		for(const RenderPacketWorkerBuffer& buffer : workerBuffers){
			orderedWorkers.push_back(&buffer);
		}
		std::sort(
			orderedWorkers.begin(),
			orderedWorkers.end(),
			[](const auto* lhs, const auto* rhs){
				return lhs->WorkerIndex() < rhs->WorkerIndex();
			}
		);

		size_t packetCount = 0;
		for(const auto* worker : orderedWorkers){
			packetCount += worker->Packets().size();
		}
		m_packets.reserve(packetCount);

		for(const auto* worker : orderedWorkers){
			m_packets.insert(
				m_packets.end(),
				worker->Packets().begin(),
				worker->Packets().end()
			);
		}

		std::stable_sort(m_packets.begin(), m_packets.end(), RenderPacketLess);
		m_ready = true;
	}

	uint64_t Generation() const noexcept { return m_generation; }
	bool IsReady() const noexcept { return m_ready; }
	const std::vector<RenderPacket>& Packets() const noexcept { return m_packets; }
	size_t Size() const noexcept { return m_packets.size(); }

	size_t Count(RenderPacketKind kind) const noexcept {
		return static_cast<size_t>(std::count_if(
			m_packets.begin(),
			m_packets.end(),
			[kind](const RenderPacket& packet){ return packet.kind == kind; }
		));
	}

private:
	uint64_t m_generation = 0;
	bool m_ready = false;
	std::vector<RenderPacket> m_packets;
};
