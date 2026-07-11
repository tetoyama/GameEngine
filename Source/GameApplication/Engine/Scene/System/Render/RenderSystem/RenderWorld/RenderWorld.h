// =======================================================================
//
// RenderWorld.h
//
// Step 18-A: ECS Worldから抽出されたFrame-local描画データの所有境界。
// Dynamic Packet、Static Batch派生データ、View Cullingを同じ世代で管理する。
//
// =======================================================================
#pragma once

#include <cstdint>
#include <span>

#include "Scene/System/Render/Culling/RenderPacketViewCulling.h"
#include "Scene/System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"

class RenderWorld final {
public:
	RenderWorld() = default;
	RenderWorld(const RenderWorld&) = delete;
	RenderWorld& operator=(const RenderWorld&) = delete;
	RenderWorld(RenderWorld&&) = delete;
	RenderWorld& operator=(RenderWorld&&) = delete;

	void BeginFrame(std::uint64_t generation){
		m_packets.BeginFrame(generation);
		m_visibility.BeginFrame(generation);
	}

	void Publish(std::span<const RenderPacketWorkerBuffer> workerBuffers){
		m_packets.Merge(workerBuffers);
	}

	void Reset() noexcept {
		m_packets.BeginFrame(0);
		m_packets.Reset();
		m_visibility.BeginFrame(0);
		m_lastSubmittedGeneration = 0;
	}

	bool MarkSubmitted() noexcept {
		if(!m_packets.IsReady()) return false;
		m_lastSubmittedGeneration = m_packets.Generation();
		return true;
	}

	bool IsReady() const noexcept {
		return m_packets.IsReady();
	}

	bool IsCurrentFrameSubmitted() const noexcept {
		return m_packets.IsReady() &&
			m_lastSubmittedGeneration == m_packets.Generation();
	}

	std::uint64_t Generation() const noexcept {
		return m_packets.Generation();
	}

	std::uint64_t LastSubmittedGeneration() const noexcept {
		return m_lastSubmittedGeneration;
	}

	// Step 18-A移行中だけ使用する互換参照。
	// RenderSystem.cppの直接代入を維持しつつ、提出世代の実体はRenderWorldが所有する。
	// RenderSystem.cppがMarkSubmitted()へ移行した段階で削除する。
	std::uint64_t& SubmissionGenerationStorage() noexcept {
		return m_lastSubmittedGeneration;
	}

	RenderPacketFrameBuffer& Packets() noexcept {
		return m_packets;
	}

	const RenderPacketFrameBuffer& Packets() const noexcept {
		return m_packets;
	}

	CullingVisibilitySet& Visibility() noexcept {
		return m_visibility;
	}

	const CullingVisibilitySet& Visibility() const noexcept {
		return m_visibility;
	}

	const StaticBatchCandidateStorage& StaticBatchCandidates() const noexcept {
		return m_packets.StaticBatchCandidates();
	}

	const StaticBatchPacketCache& StaticBatchCache() const noexcept {
		return m_packets.StaticBatchCache();
	}

	const StaticBatchInstanceDataBuffer& StaticBatchInstances() const noexcept {
		return m_packets.StaticBatchInstances();
	}

	void PrepareView(const RenderPacketCullingView& view){
		RenderPacketViewCulling::Prepare(m_visibility, m_packets, view);
	}

	bool ShouldRender(
		const RenderPacketCullingView& view,
		const RenderPacket& packet
	) const {
		return RenderPacketViewCulling::ShouldRender(
			m_visibility,
			view,
			packet
		);
	}

private:
	RenderPacketFrameBuffer m_packets;
	CullingVisibilitySet m_visibility;
	std::uint64_t m_lastSubmittedGeneration = 0;
};
