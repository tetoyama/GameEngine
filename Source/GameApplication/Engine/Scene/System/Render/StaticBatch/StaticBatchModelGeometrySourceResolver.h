#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>

#include "Scene/Component/modelRendererComponent.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacket.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchPacketCache.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchResourceKey.h"
#include "System/Render/StaticBatch/StaticBatchD3D11GeometrySource.h"
#include "Shader/common.hlsl"

enum class StaticBatchModelGeometryRejectReason : std::uint8_t {
	None,
	InvalidRepresentativePacket,
	GroupPacketMismatch,
	UnsupportedPacketKind,
	UnsupportedRenderLayer,
	MissingGBufferPass,
	MissingModelRenderer,
	AnimatedModel,
	MissingModelResource,
	UnsupportedSubMeshCount,
	MissingSubMesh,
	SkinnedSubMesh,
	MissingNativeBuffer,
	InvalidGeometryCount,
	GeometryResourceKeyMismatch
};

struct StaticBatchModelGeometryResolveResult {
	StaticBatchD3D11GeometrySource source;
	StaticBatchModelGeometryRejectReason rejectReason =
		StaticBatchModelGeometryRejectReason::InvalidRepresentativePacket;

	bool IsEligible() const noexcept {
		return rejectReason == StaticBatchModelGeometryRejectReason::None &&
			source.IsValid();
	}
};

namespace StaticBatchModelGeometrySourceResolver {

inline bool UsesDynamicVertexBuffer(
	const ModelRendererComponent& renderer
) noexcept {
	return std::any_of(
		renderer.dynamicVertexBuffers.begin(),
		renderer.dynamicVertexBuffers.end(),
		[](const ID3D11Buffer* buffer){ return buffer != nullptr; }
	);
}

inline StaticBatchModelGeometryResolveResult Resolve(
	const StaticBatchPacketCacheEntry& group,
	std::span<const RenderPacket> packets
) noexcept {
	StaticBatchModelGeometryResolveResult result;
	if(group.representativePacketIndex >= packets.size()){
		return result;
	}

	const RenderPacket& packet = packets[group.representativePacketIndex];
	if(packet.sceneContextID != group.sceneContextID ||
		packet.kind != group.key.kind){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::GroupPacketMismatch;
		return result;
	}
	if(packet.kind != RenderPacketKind::Model){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::UnsupportedPacketKind;
		return result;
	}
	if(packet.layer != RenderLayer::Opaque3D){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::UnsupportedRenderLayer;
		return result;
	}
	if(!HasRenderPacketPass(packet.passMask, RenderPacketPassMask::GBuffer)){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::MissingGBufferPass;
		return result;
	}

	const ModelRendererComponent* renderer = packet.bindings.modelRenderer;
	if(!renderer){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::MissingModelRenderer;
		return result;
	}
	if(!renderer->blendedAnimations.empty() || UsesDynamicVertexBuffer(*renderer)){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::AnimatedModel;
		return result;
	}

	const std::shared_ptr<ModelData>& model = renderer->model;
	if(!model || !model->AiScene){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::MissingModelResource;
		return result;
	}
	if(model->AiScene->mNumMeshes != 1){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::UnsupportedSubMeshCount;
		return result;
	}
	if(!model->AiScene->mMeshes || !model->AiScene->mMeshes[0]){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::MissingSubMesh;
		return result;
	}

	const aiMesh* mesh = model->AiScene->mMeshes[0];
	if(mesh->HasBones()){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::SkinnedSubMesh;
		return result;
	}
	if(model->VertexBuffer.empty() || model->IndexBuffer.empty() ||
		!model->VertexBuffer[0] || !model->IndexBuffer[0]){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::MissingNativeBuffer;
		return result;
	}
	if(mesh->mNumVertices == 0 || mesh->mNumFaces == 0 ||
		mesh->mNumFaces >
			(std::numeric_limits<std::uint32_t>::max)() / 3u){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::InvalidGeometryCount;
		return result;
	}

	const std::uint64_t geometryResourceKey =
		StaticBatchResourceKey::MakeGeometryKey(packet);
	if(geometryResourceKey == 0 ||
		geometryResourceKey != group.key.geometryKey){
		result.rejectReason =
			StaticBatchModelGeometryRejectReason::GeometryResourceKeyMismatch;
		return result;
	}

	result.source.vertexBuffer = model->VertexBuffer[0];
	result.source.indexBuffer = model->IndexBuffer[0];
	result.source.vertexStride =
		static_cast<std::uint32_t>(sizeof(VERTEX_3D));
	result.source.vertexCount = mesh->mNumVertices;
	result.source.indexCount = mesh->mNumFaces * 3u;
	result.source.indexFormat = RHI::IndexFormat::UInt32;
	result.source.geometryResourceKey = geometryResourceKey;
	result.rejectReason = result.source.IsValid()
		? StaticBatchModelGeometryRejectReason::None
		: StaticBatchModelGeometryRejectReason::InvalidGeometryCount;
	return result;
}

} // namespace StaticBatchModelGeometrySourceResolver
