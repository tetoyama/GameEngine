#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "RenderPacket.h"
#include "Scene/scene.h"
#include "Scene/Registry/componentRegistry.h"
#include "Scene/Component/EntityStateComponents.h"

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
	size_t Size() const noexcept { return m_packets.size(); }
	size_t Capacity() const noexcept { return m_packets.capacity(); }

private:
	uint32_t m_workerIndex = 0;
	std::vector<RenderPacket> m_packets;
};

struct RenderPacketStorageTelemetry {
	size_t currentSize = 0;
	size_t peakSize = 0;
	size_t capacity = 0;
	size_t growthEventCount = 0;
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

	// Explicit initial reserve is configuration, not a runtime growth event.
	void Reserve(size_t count){
		m_packets.reserve(count);
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
		size_t configuredReserve = 0;
		std::vector<SceneContext*> reservedScenes;
		for(const auto* worker : orderedWorkers){
			packetCount += worker->Packets().size();
			for(const RenderPacket& packet : worker->Packets()){
				SceneContext* context = packet.bindings.sceneContext;
				if(!context) continue;
				if(std::find(reservedScenes.begin(), reservedScenes.end(), context) !=
					reservedScenes.end()){
					continue;
				}
				reservedScenes.push_back(context);
				configuredReserve += static_cast<size_t>(
					context->storageConfig.renderPacketReserve
				);
			}
		}

		const size_t capacityBefore = m_packets.capacity();
		const size_t reserveTarget = (std::max)(packetCount, configuredReserve);
		m_packets.reserve(reserveTarget);
		if(m_packets.capacity() > capacityBefore &&
			packetCount > configuredReserve &&
			packetCount > capacityBefore){
			++m_growthEventCount;
		}

		for(const auto* worker : orderedWorkers){
			for(const RenderPacket& packet : worker->Packets()){
				if(!ShouldPublish(packet)) continue;
				m_packets.push_back(packet);
			}
		}

		std::stable_sort(m_packets.begin(), m_packets.end(), RenderPacketLess);
		m_peakSize = (std::max)(m_peakSize, m_packets.size());
		m_ready = true;
	}

	uint64_t Generation() const noexcept { return m_generation; }
	bool IsReady() const noexcept { return m_ready; }
	const std::vector<RenderPacket>& Packets() const noexcept { return m_packets; }
	size_t Size() const noexcept { return m_packets.size(); }
	size_t Capacity() const noexcept { return m_packets.capacity(); }
	size_t PeakSize() const noexcept { return m_peakSize; }
	size_t GrowthEventCount() const noexcept { return m_growthEventCount; }

	RenderPacketStorageTelemetry Telemetry() const noexcept {
		return RenderPacketStorageTelemetry{
			m_packets.size(),
			m_peakSize,
			m_packets.capacity(),
			m_growthEventCount
		};
	}

	void ResetPeakMetrics() noexcept {
		m_peakSize = m_packets.size();
		m_growthEventCount = 0;
	}

	size_t Count(RenderPacketKind kind) const noexcept {
		return static_cast<size_t>(std::count_if(
			m_packets.begin(),
			m_packets.end(),
			[kind](const RenderPacket& packet){ return packet.kind == kind; }
		));
	}

private:
	static bool ShouldPublish(const RenderPacket& packet){
		SceneContext* context = packet.bindings.sceneContext;
		if(!context || !context->entity || !context->component){
			return false;
		}
		if(!context->entity->IsAlive(packet.entity)){
			return false;
		}

		ComponentRegistry* registry = context->component;

		// Auto-registration経路でもDisabledを確実に除外する。
		// default query exclusion maskはScene初期登録後の高速経路として維持する。
		if(registry->HasComponent<DisabledComponent>(packet.entity)){
			return false;
		}
		if(!registry->IsEntityEnabledForDefaultQueries(packet.entity)){
			return false;
		}

		return !registry->HasComponent<HiddenComponent>(packet.entity);
	}

	uint64_t m_generation = 0;
	bool m_ready = false;
	std::vector<RenderPacket> m_packets;
	size_t m_peakSize = 0;
	size_t m_growthEventCount = 0;
};
