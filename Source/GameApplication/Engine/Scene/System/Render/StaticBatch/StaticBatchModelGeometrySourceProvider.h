#pragma once

#include <cstdint>
#include <limits>

#include "Scene/Component/modelRendererComponent.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacket.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacketModelSubMeshSelection.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchResourceKey.h"
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
// 現在はLegacy Providerが既存ModelDataを参照するが、RenderSystem側Runtime
// Storageへ移行する際はこのInterfaceの実装を差し替え、Resolver / Cacheの
// 契約を変更しない。
class IStaticBatchModelGeometrySourceProvider {
public:
	virtual ~IStaticBatchModelGeometrySourceProvider() = default;

	virtual StaticBatchModelGeometrySourceResult Resolve(
		const ModelRendererComponent& renderer,
		const RenderPacket& packet
	) const noexcept = 0;
};

class StaticBatchLegacyModelGeometrySourceProvider final
	: public IStaticBatchModelGeometrySourceProvider {
public:
	StaticBatchModelGeometrySourceResult Resolve(
		const ModelRendererComponent& renderer,
		const RenderPacket& packet
	) const noexcept override {
		StaticBatchModelGeometrySourceResult result;
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
		if(meshIndex >= model->VertexBuffer.size() ||
			meshIndex >= model->IndexBuffer.size() ||
			!model->VertexBuffer[meshIndex] ||
			!model->IndexBuffer[meshIndex]){
			result.status = StaticBatchModelGeometrySourceStatus::MissingNativeBuffer;
			return result;
		}
		if(mesh->mNumVertices == 0 || mesh->mNumFaces == 0 ||
			mesh->mNumFaces >
				(std::numeric_limits<std::uint32_t>::max)() / 3u){
			result.status = StaticBatchModelGeometrySourceStatus::InvalidGeometryCount;
			return result;
		}

		const std::uint64_t geometryResourceKey =
			StaticBatchResourceKey::MakeGeometryKey(packet);
		if(geometryResourceKey == 0){
			result.status = StaticBatchModelGeometrySourceStatus::InvalidGeometryCount;
			return result;
		}

		result.source.vertexBuffer = model->VertexBuffer[meshIndex];
		result.source.indexBuffer = model->IndexBuffer[meshIndex];
		result.source.vertexStride =
			static_cast<std::uint32_t>(sizeof(VERTEX_3D));
		result.source.vertexCount = mesh->mNumVertices;
		result.source.indexCount = mesh->mNumFaces * 3u;
		result.source.indexFormat = RHI::IndexFormat::UInt32;
		result.source.geometryResourceKey = geometryResourceKey;
		result.status = result.source.IsValid()
			? StaticBatchModelGeometrySourceStatus::None
			: StaticBatchModelGeometrySourceStatus::InvalidGeometryCount;
		return result;
	}
};

namespace StaticBatchModelGeometrySourceProviders {

inline const IStaticBatchModelGeometrySourceProvider& LegacyModelData() noexcept {
	static const StaticBatchLegacyModelGeometrySourceProvider provider;
	return provider;
}

} // namespace StaticBatchModelGeometrySourceProviders
