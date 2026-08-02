#pragma once

#include <cstdint>
#include <limits>
#include <span>

#include "Scene/Component/modelRendererComponent.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacket.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacketModelSubMeshSelection.h"
#include "System/Render/StaticBatch/StaticBatchD3D11GeometrySource.h"
#include "Shader/common.hlsl"

enum class StaticBatchModelGeometrySourceStatus : std::uint8_t {
	None,
	MissingModelResource,
	UnsupportedSubMeshCount,
	InvalidSubMeshIndex,
	MissingSubMesh,
	SkinnedSubMesh,
	MissingNativeBuffer,
	InvalidGeometryCount
};

struct StaticBatchModelGeometrySourceResult {
	StaticBatchD3D11GeometrySource source;
	StaticBatchModelGeometrySourceStatus status =
		StaticBatchModelGeometrySourceStatus::MissingModelResource;

	bool IsEligible() const noexcept {
		return status == StaticBatchModelGeometrySourceStatus::None &&
			source.IsValid();
	}
};

// Static BatchのGroup解決からModelDataのNative Buffer所有形態を分離する境界。
// Geometry KeyはPacket Build時に確定したGroup Keyを受け取り、Provider内で
// ModelDataを再走査してKeyを再生成しない。
class IStaticBatchModelGeometrySourceProvider {
public:
	virtual ~IStaticBatchModelGeometrySourceProvider() = default;

	virtual StaticBatchModelGeometrySourceResult Resolve(
		const ModelRendererComponent& renderer,
		const RenderPacket& packet,
		std::uint64_t expectedGeometryResourceKey
	) const noexcept = 0;
};

// ModelData内のBackend非依存CPU Geometry Snapshotを公開するProvider。
// Static Batch用RHI BufferはこのSourceから生成し、ModelDataのLegacy Native
// Vertex / Index BufferをBootstrapに使用しない。
class StaticBatchModelCpuGeometrySourceProvider final
	: public IStaticBatchModelGeometrySourceProvider {
public:
	StaticBatchModelGeometrySourceResult Resolve(
		const ModelRendererComponent& renderer,
		const RenderPacket& packet,
		std::uint64_t expectedGeometryResourceKey
	) const noexcept override {
		StaticBatchModelGeometrySourceResult result;
		if(expectedGeometryResourceKey == 0){
			result.status =
				StaticBatchModelGeometrySourceStatus::InvalidGeometryCount;
			return result;
		}

		const std::shared_ptr<ModelData>& model = renderer.model;
		if(!model || !model->AiScene){
			return result;
		}
		if(!model->AiScene->mMeshes){
			result.status = StaticBatchModelGeometrySourceStatus::MissingSubMesh;
			return result;
		}

		std::uint32_t meshIndex = 0;
		if(!RenderPacketModelSubMeshSelection::ResolveSingleIndex(
			packet,
			model->AiScene->mNumMeshes,
			meshIndex
		)){
			result.status = packet.TargetsAllSubMeshes()
				? StaticBatchModelGeometrySourceStatus::UnsupportedSubMeshCount
				: StaticBatchModelGeometrySourceStatus::InvalidSubMeshIndex;
			return result;
		}

		const aiMesh* mesh = model->AiScene->mMeshes[meshIndex];
		if(!mesh){
			result.status = StaticBatchModelGeometrySourceStatus::MissingSubMesh;
			return result;
		}
		if(mesh->HasBones()){
			result.status = StaticBatchModelGeometrySourceStatus::SkinnedSubMesh;
			return result;
		}
		if(meshIndex >= model->MeshGeometry.size()){
			result.status = StaticBatchModelGeometrySourceStatus::InvalidGeometryCount;
			return result;
		}

		const ModelMeshGeometryCpuData& geometry = model->MeshGeometry[meshIndex];
		if(!geometry.IsValid() ||
			geometry.vertices.size() >
				(std::numeric_limits<std::uint32_t>::max)() ||
			geometry.indices.size() >
				(std::numeric_limits<std::uint32_t>::max)()){
			result.status = StaticBatchModelGeometrySourceStatus::InvalidGeometryCount;
			return result;
		}

		result.source.vertexData = std::as_bytes(
			std::span<const VERTEX_3D>(
				geometry.vertices.data(),
				geometry.vertices.size()
			)
		);
		result.source.indexData = std::as_bytes(
			std::span<const std::uint32_t>(
				geometry.indices.data(),
				geometry.indices.size()
			)
		);
		result.source.vertexStride =
			static_cast<std::uint32_t>(sizeof(VERTEX_3D));
		result.source.vertexCount =
			static_cast<std::uint32_t>(geometry.vertices.size());
		result.source.indexCount =
			static_cast<std::uint32_t>(geometry.indices.size());
		result.source.indexFormat = RHI::IndexFormat::UInt32;
		result.source.geometryResourceKey = expectedGeometryResourceKey;
		result.source.sourceRevision = model->GetGeometryRevision();
		result.status = result.source.IsValid()
			? StaticBatchModelGeometrySourceStatus::None
			: StaticBatchModelGeometrySourceStatus::InvalidGeometryCount;
		return result;
	}
};

namespace StaticBatchModelGeometrySourceProviders {

inline const IStaticBatchModelGeometrySourceProvider& ModelCpuData() noexcept {
	static const StaticBatchModelCpuGeometrySourceProvider provider;
	return provider;
}

// 段階移行中のSource互換名。実体はNative Buffer ProviderではなくCPU Provider。
inline const IStaticBatchModelGeometrySourceProvider& LegacyModelData() noexcept {
	return ModelCpuData();
}

} // namespace StaticBatchModelGeometrySourceProviders
