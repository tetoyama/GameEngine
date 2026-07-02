#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "RenderPacket.h"
#include "StaticBatchCandidateTypes.h"
#include "StaticBatchPacketCache.h"
#include "StaticBatchInstanceDataBuffer.h"
#include "StaticBatchResourceKey.h"
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
	void Add(RenderPacket packet){ m_packets.emplace_back(std::move(packet)); }
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
	size_t safetyGrowthOverrideCount = 0;
	bool lastMergeUsedSafetyGrowth = false;
};

class RenderPacketFrameBuffer {
public:
	void BeginFrame(uint64_t generation){
		m_generation = generation;
		m_packets.clear();
		m_staticBatchCandidates.BeginFrame();
		m_lastMergeUsedSafetyGrowth = false;
		m_ready = false;
	}

	void Reset() noexcept {
		m_packets.clear();
		m_staticBatchCandidates.Reset();
		m_staticBatchPacketCache.Reset();
		m_staticBatchInstanceData.Reset();
		m_lastMergeUsedSafetyGrowth = false;
		m_ready = false;
	}

	void Reserve(size_t count){ m_packets.reserve(count); }
	void ReserveStaticBatchCandidates(size_t count){
		m_staticBatchCandidates.Reserve(count);
		m_staticBatchPacketCache.Reserve(count);
		m_staticBatchInstanceData.Reserve(count);
	}

	void Merge(std::span<const RenderPacketWorkerBuffer> workerBuffers){
		m_lastMergeUsedSafetyGrowth = false;

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

		size_t publishablePacketCount = 0;
		size_t configuredPacketReserve = 0;
		size_t configuredStaticBatchReserve = 0;
		bool allowPacketGrowth = true;
		bool allowStaticBatchGrowth = true;
		std::vector<SceneContext*> reservedScenes;
		for(const auto* worker : orderedWorkers){
			for(const RenderPacket& packet : worker->Packets()){
				if(ShouldPublish(packet)){
					publishablePacketCount += PublishedPacketMultiplicity(packet);
				}

				SceneContext* context = packet.bindings.sceneContext;
				if(!context) continue;
				if(std::find(reservedScenes.begin(), reservedScenes.end(), context) !=
					reservedScenes.end()){
					continue;
				}
				reservedScenes.push_back(context);
				configuredPacketReserve += static_cast<size_t>(
					context->storageConfig.renderPacketReserve
				);
				configuredStaticBatchReserve += static_cast<size_t>(
					context->storageConfig.staticBatchReserve
				);
				allowPacketGrowth =
					allowPacketGrowth &&
					context->storageConfig.allowRuntimeGrowth;
				allowStaticBatchGrowth =
					allowStaticBatchGrowth &&
					context->storageConfig.allowRuntimeGrowth;
			}
		}

		if(configuredPacketReserve > m_packets.capacity()){
			m_packets.reserve(configuredPacketReserve);
		}

		if(publishablePacketCount > m_packets.capacity()){
			m_packets.reserve(publishablePacketCount);
			++m_growthEventCount;
			if(!allowPacketGrowth){
				++m_safetyGrowthOverrideCount;
				m_lastMergeUsedSafetyGrowth = true;
			}
		}

		m_staticBatchCandidates.Reserve(configuredStaticBatchReserve);
		m_staticBatchCandidates.SetRuntimeGrowthAllowed(allowStaticBatchGrowth);
		m_staticBatchPacketCache.Reserve(configuredStaticBatchReserve);
		m_staticBatchInstanceData.Reserve(configuredStaticBatchReserve);

		for(const auto* worker : orderedWorkers){
			for(const RenderPacket& packet : worker->Packets()){
				if(!ShouldPublish(packet)) continue;
				AppendPublishedPacket(packet);
			}
		}

		std::stable_sort(m_packets.begin(), m_packets.end(), RenderPacketLess);
		CollectStaticBatchCandidates();
		m_staticBatchPacketCache.Synchronize(
			m_staticBatchCandidates,
			m_packets,
			m_generation,
			allowStaticBatchGrowth
		);
		m_staticBatchInstanceData.Synchronize(
			m_staticBatchPacketCache,
			allowStaticBatchGrowth
		);
		m_peakSize = (std::max)(m_peakSize, m_packets.size());
		m_ready = true;
	}

	uint64_t Generation() const noexcept { return m_generation; }
	bool IsReady() const noexcept { return m_ready; }
	const std::vector<RenderPacket>& Packets() const noexcept { return m_packets; }
	const StaticBatchCandidateStorage& StaticBatchCandidates() const noexcept {
		return m_staticBatchCandidates;
	}
	const StaticBatchPacketCache& StaticBatchCache() const noexcept {
		return m_staticBatchPacketCache;
	}
	const StaticBatchInstanceDataBuffer& StaticBatchInstances() const noexcept {
		return m_staticBatchInstanceData;
	}
	size_t Size() const noexcept { return m_packets.size(); }
	size_t Capacity() const noexcept { return m_packets.capacity(); }
	size_t PeakSize() const noexcept { return m_peakSize; }
	size_t GrowthEventCount() const noexcept { return m_growthEventCount; }
	size_t SafetyGrowthOverrideCount() const noexcept {
		return m_safetyGrowthOverrideCount;
	}
	bool LastMergeUsedSafetyGrowth() const noexcept {
		return m_lastMergeUsedSafetyGrowth;
	}

	RenderPacketStorageTelemetry Telemetry() const noexcept {
		return {
			m_packets.size(),
			m_peakSize,
			m_packets.capacity(),
			m_growthEventCount,
			m_safetyGrowthOverrideCount,
			m_lastMergeUsedSafetyGrowth
		};
	}

	StaticBatchCandidateStorageTelemetry StaticBatchTelemetry() const noexcept {
		return m_staticBatchCandidates.Telemetry();
	}

	StaticBatchPacketCacheTelemetry StaticBatchCacheTelemetry() const noexcept {
		return m_staticBatchPacketCache.Telemetry();
	}

	StaticBatchInstanceDataTelemetry StaticBatchInstanceTelemetry() const noexcept {
		return m_staticBatchInstanceData.Telemetry();
	}

	void ResetPeakMetrics() noexcept {
		m_peakSize = m_packets.size();
		m_growthEventCount = 0;
		m_safetyGrowthOverrideCount = 0;
		m_lastMergeUsedSafetyGrowth = false;
		m_staticBatchCandidates.ResetPeakMetrics();
		m_staticBatchPacketCache.ResetPeakMetrics();
		m_staticBatchInstanceData.ResetPeakMetrics();
	}

	size_t Count(RenderPacketKind kind) const noexcept {
		return static_cast<size_t>(std::count_if(
			m_packets.begin(),
			m_packets.end(),
			[kind](const RenderPacket& packet){ return packet.kind == kind; }
		));
	}

private:
	static std::uint32_t ResolvedModelSubMeshCount(
		const RenderPacket& packet
	) noexcept {
		if(packet.kind != RenderPacketKind::Model ||
			!packet.TargetsAllSubMeshes() ||
			!packet.bindings.modelRenderer){
			return 0;
		}
		const std::shared_ptr<ModelData>& model =
			packet.bindings.modelRenderer->model;
		if(!model || !model->AiScene || !model->AiScene->mMeshes ||
			model->AiScene->mNumMeshes == 0){
			return 0;
		}
		return model->AiScene->mNumMeshes;
	}

	static size_t PublishedPacketMultiplicity(
		const RenderPacket& packet
	) noexcept {
		const std::uint32_t subMeshCount = ResolvedModelSubMeshCount(packet);
		return subMeshCount != 0
			? static_cast<size_t>(subMeshCount)
			: size_t{1};
	}

	void AppendPublishedPacket(const RenderPacket& packet){
		const std::uint32_t subMeshCount = ResolvedModelSubMeshCount(packet);
		if(subMeshCount == 0){
			m_packets.push_back(packet);
			return;
		}

		for(std::uint32_t subMeshIndex = 0;
			subMeshIndex < subMeshCount;
			++subMeshIndex){
			RenderPacket scopedPacket = packet;
			scopedPacket.subMeshIndex = subMeshIndex;
			m_packets.push_back(std::move(scopedPacket));
		}
	}

	static bool IsStaticBatchEligible(const RenderPacket& packet) noexcept {
		return packet.kind == RenderPacketKind::Model ||
			packet.kind == RenderPacketKind::Mesh;
	}

	void CollectStaticBatchCandidates(){
		for(size_t packetIndex = 0; packetIndex < m_packets.size(); ++packetIndex){
			const RenderPacket& packet = m_packets[packetIndex];
			SceneContext* context = packet.bindings.sceneContext;
			if(!context || !context->component) continue;
			if(!IsStaticBatchEligible(packet)) continue;
			if(!context->component->HasComponent<StaticEntityComponent>(packet.entity)){
				continue;
			}

			const StaticBatchResourceKeySet resourceKeys =
				StaticBatchResourceKey::Build(packet);
			if(!m_staticBatchCandidates.Add({
				{
					packet.kind,
					packet.layer,
					packet.materialKey,
					resourceKeys.pipelineKey,
					resourceKeys.geometryKey,
					resourceKeys.textureSetKey,
					resourceKeys.materialStateKey
				},
				packet.sceneContextID,
				packet.entity,
				packetIndex,
				packet.stableSequence
			})){
				break;
			}
		}
		m_staticBatchCandidates.Sort();
	}

	static bool ShouldPublish(const RenderPacket& packet){
		SceneContext* context = packet.bindings.sceneContext;
		if(!context || !context->entity || !context->component){
			return false;
		}
		if(!context->entity->IsAlive(packet.entity)){
			return false;
		}

		ComponentRegistry* registry = context->component;
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
	StaticBatchCandidateStorage m_staticBatchCandidates;
	StaticBatchPacketCache m_staticBatchPacketCache;
	StaticBatchInstanceDataBuffer m_staticBatchInstanceData;
	size_t m_peakSize = 0;
	size_t m_growthEventCount = 0;
	size_t m_safetyGrowthOverrideCount = 0;
	bool m_lastMergeUsedSafetyGrowth = false;
};
