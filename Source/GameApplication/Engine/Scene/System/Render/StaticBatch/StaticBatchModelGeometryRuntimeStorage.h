#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <vector>

#include <wrl/client.h>

#include "System/Render/StaticBatch/StaticBatchModelGeometrySourceProvider.h"

struct StaticBatchModelGeometryRuntimeStorageTelemetry {
	std::size_t currentEntryCount = 0;
	std::size_t peakEntryCount = 0;
	std::size_t synchronizationCount = 0;
	std::size_t importCount = 0;
	std::size_t reuseCount = 0;
	std::size_t replacementCount = 0;
	std::size_t releaseCount = 0;
	std::size_t rejectedSourceCount = 0;
};

// ModelDataが生成したD3D11 GeometryをStatic Batch Runtime側で保持する。
// EntryはVertex / Index Bufferへ独立したCOM参照を持つため、Geometry Binding
// Cacheの同期中にComponentやModelDataの寿命へ依存しない。
class StaticBatchModelGeometryRuntimeStorage {
public:
	StaticBatchModelGeometryRuntimeStorage() = default;
	StaticBatchModelGeometryRuntimeStorage(
		const StaticBatchModelGeometryRuntimeStorage&
	) = delete;
	StaticBatchModelGeometryRuntimeStorage& operator=(
		const StaticBatchModelGeometryRuntimeStorage&
	) = delete;

	void BeginSynchronization(){
		m_activeKeys.clear();
		++m_synchronizationCount;
	}

	StaticBatchModelGeometrySourceResult Resolve(
		const ModelRendererComponent& renderer,
		const RenderPacket& packet,
		std::uint64_t expectedGeometryResourceKey,
		const IStaticBatchModelGeometrySourceProvider& bootstrapProvider
	){
		StaticBatchModelGeometrySourceResult result;
		if(expectedGeometryResourceKey == 0){
			result.status =
				StaticBatchModelGeometrySourceStatus::InvalidGeometryCount;
			++m_rejectedSourceCount;
			return result;
		}

		auto entryIt = FindEntry(expectedGeometryResourceKey);
		if(entryIt != m_entries.end() && entryIt->Matches(renderer, packet)){
			AddActiveKey(expectedGeometryResourceKey);
			++m_reuseCount;
			return entryIt->MakeResult();
		}

		// 同じ同期内で同一Keyが別Model実体へ解決された場合、先に採用した
		// Entryを維持する。ここで置換するとGroup順によってSourceが往復する。
		if(entryIt != m_entries.end() &&
			Contains(m_activeKeys, expectedGeometryResourceKey)){
			result.status =
				StaticBatchModelGeometrySourceStatus::InvalidGeometryCount;
			++m_rejectedSourceCount;
			return result;
		}

		const StaticBatchModelGeometrySourceResult bootstrapResult =
			bootstrapProvider.Resolve(
				renderer,
				packet,
				expectedGeometryResourceKey
			);
		if(!bootstrapResult.IsEligible() ||
			bootstrapResult.source.geometryResourceKey !=
				expectedGeometryResourceKey){
			++m_rejectedSourceCount;
			return bootstrapResult;
		}

		Entry replacement;
		if(!replacement.Initialize(
			renderer,
			packet,
			bootstrapResult.source
		)){
			result.status =
				StaticBatchModelGeometrySourceStatus::MissingNativeBuffer;
			++m_rejectedSourceCount;
			return result;
		}

		if(entryIt != m_entries.end()){
			*entryIt = std::move(replacement);
			++m_replacementCount;
		}else{
			m_entries.push_back(std::move(replacement));
			entryIt = std::prev(m_entries.end());
			++m_importCount;
		}

		AddActiveKey(expectedGeometryResourceKey);
		m_peakEntryCount = (std::max)(m_peakEntryCount, m_entries.size());
		return entryIt->MakeResult();
	}

	void EndSynchronization(){
		for(auto entryIt = m_entries.begin(); entryIt != m_entries.end();){
			if(Contains(m_activeKeys, entryIt->geometryResourceKey)){
				++entryIt;
				continue;
			}
			entryIt = m_entries.erase(entryIt);
			++m_releaseCount;
		}
		m_activeKeys.clear();
	}

	void Reset() noexcept {
		m_releaseCount += m_entries.size();
		m_entries.clear();
		m_activeKeys.clear();
	}

	std::size_t EntryCount() const noexcept {
		return m_entries.size();
	}

	StaticBatchModelGeometryRuntimeStorageTelemetry Telemetry() const noexcept {
		return {
			m_entries.size(),
			m_peakEntryCount,
			m_synchronizationCount,
			m_importCount,
			m_reuseCount,
			m_replacementCount,
			m_releaseCount,
			m_rejectedSourceCount
		};
	}

	void ResetMetrics() noexcept {
		m_peakEntryCount = m_entries.size();
		m_synchronizationCount = 0;
		m_importCount = 0;
		m_reuseCount = 0;
		m_replacementCount = 0;
		m_releaseCount = 0;
		m_rejectedSourceCount = 0;
	}

private:
	struct Entry {
		std::uint64_t geometryResourceKey = 0;
		std::weak_ptr<ModelData> modelIdentity;
		std::uint64_t modelRuntimeRevision = 0;
		std::uint32_t subMeshIndex = RenderPacketAllSubMeshes;
		bool targetsAllSubMeshes = true;
		Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
		std::uint32_t vertexStride = 0;
		std::uint32_t vertexCount = 0;
		std::uint32_t indexCount = 0;
		RHI::IndexFormat indexFormat = RHI::IndexFormat::UInt32;

		bool Initialize(
			const ModelRendererComponent& renderer,
			const RenderPacket& packet,
			const StaticBatchD3D11GeometrySource& source
		){
			if(!source.IsValid() || !renderer.model) return false;

			Microsoft::WRL::ComPtr<ID3D11Buffer> newVertexBuffer;
			Microsoft::WRL::ComPtr<ID3D11Buffer> newIndexBuffer;
			newVertexBuffer = source.vertexBuffer;
			newIndexBuffer = source.indexBuffer;
			if(!newVertexBuffer || !newIndexBuffer) return false;

			geometryResourceKey = source.geometryResourceKey;
			modelIdentity = renderer.model;
			modelRuntimeRevision = renderer.modelRuntimeRevision;
			subMeshIndex = packet.subMeshIndex;
			targetsAllSubMeshes = packet.TargetsAllSubMeshes();
			vertexBuffer = std::move(newVertexBuffer);
			indexBuffer = std::move(newIndexBuffer);
			vertexStride = source.vertexStride;
			vertexCount = source.vertexCount;
			indexCount = source.indexCount;
			indexFormat = source.indexFormat;
			return true;
		}

		bool Matches(
			const ModelRendererComponent& renderer,
			const RenderPacket& packet
		) const noexcept {
			const std::shared_ptr<ModelData> model = modelIdentity.lock();
			return model && model == renderer.model &&
				modelRuntimeRevision == renderer.modelRuntimeRevision &&
				subMeshIndex == packet.subMeshIndex &&
				targetsAllSubMeshes == packet.TargetsAllSubMeshes() &&
				vertexBuffer && indexBuffer;
		}

		StaticBatchModelGeometrySourceResult MakeResult() const noexcept {
			StaticBatchModelGeometrySourceResult result;
			result.source.vertexBuffer = vertexBuffer.Get();
			result.source.indexBuffer = indexBuffer.Get();
			result.source.vertexStride = vertexStride;
			result.source.vertexCount = vertexCount;
			result.source.indexCount = indexCount;
			result.source.indexFormat = indexFormat;
			result.source.geometryResourceKey = geometryResourceKey;
			result.status = result.source.IsValid()
				? StaticBatchModelGeometrySourceStatus::None
				: StaticBatchModelGeometrySourceStatus::MissingNativeBuffer;
			return result;
		}
	};

	using EntryIterator = std::vector<Entry>::iterator;

	EntryIterator FindEntry(std::uint64_t geometryResourceKey){
		return std::find_if(
			m_entries.begin(),
			m_entries.end(),
			[geometryResourceKey](const Entry& entry){
				return entry.geometryResourceKey == geometryResourceKey;
			}
		);
	}

	static bool Contains(
		std::span<const std::uint64_t> keys,
		std::uint64_t key
	) noexcept {
		return std::find(keys.begin(), keys.end(), key) != keys.end();
	}

	void AddActiveKey(std::uint64_t key){
		if(!Contains(m_activeKeys, key)) m_activeKeys.push_back(key);
	}

	std::vector<Entry> m_entries;
	std::vector<std::uint64_t> m_activeKeys;
	std::size_t m_peakEntryCount = 0;
	std::size_t m_synchronizationCount = 0;
	std::size_t m_importCount = 0;
	std::size_t m_reuseCount = 0;
	std::size_t m_replacementCount = 0;
	std::size_t m_releaseCount = 0;
	std::size_t m_rejectedSourceCount = 0;
};

// Static Batch ResolverへRuntime Storageを公開するProvider。
// Legacy ProviderはStorage Miss / Revision差し替え時のBootstrapにだけ使用する。
class StaticBatchRuntimeModelGeometrySourceProvider final
	: public IStaticBatchModelGeometrySourceProvider {
public:
	explicit StaticBatchRuntimeModelGeometrySourceProvider(
		const IStaticBatchModelGeometrySourceProvider& bootstrapProvider =
			StaticBatchModelGeometrySourceProviders::LegacyModelData()
	) noexcept
		: m_bootstrapProvider(&bootstrapProvider) {
	}

	StaticBatchModelGeometrySourceResult Resolve(
		const ModelRendererComponent& renderer,
		const RenderPacket& packet,
		std::uint64_t expectedGeometryResourceKey
	) const noexcept override {
		if(!m_bootstrapProvider){
			return {};
		}
		return m_storage.Resolve(
			renderer,
			packet,
			expectedGeometryResourceKey,
			*m_bootstrapProvider
		);
	}

	void BeginSynchronization() const {
		m_storage.BeginSynchronization();
	}

	void EndSynchronization() const {
		m_storage.EndSynchronization();
	}

	void Reset() const noexcept {
		m_storage.Reset();
	}

	const StaticBatchModelGeometryRuntimeStorage& Storage() const noexcept {
		return m_storage;
	}

	StaticBatchModelGeometryRuntimeStorage& Storage() noexcept {
		return m_storage;
	}

private:
	mutable StaticBatchModelGeometryRuntimeStorage m_storage;
	const IStaticBatchModelGeometrySourceProvider* m_bootstrapProvider = nullptr;
};
