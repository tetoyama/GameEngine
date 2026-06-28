#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>

#include "Backends/Assimp/material.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchPacketCache.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchResourceKey.h"

enum class StaticBatchModelMaterialRejectReason : std::uint8_t {
	None,
	InvalidRepresentativePacket,
	GroupPacketMismatch,
	UnsupportedPacketKind,
	UnsupportedRenderLayer,
	MissingGBufferPass,
	MissingModelRenderer,
	MissingModelResource,
	UnsupportedSubMeshCount,
	MissingSubMesh,
	InvalidMaterialIndex,
	MissingMaterial,
	ShaderKeyMismatch,
	ExcludedByGBufferAlphaRule,
	MissingOverrideDiffuseTexture,
	MissingModelDiffuseTexture,
	NormalMapUnsupported,
	ResourceKeyMismatch
};

struct StaticBatchModelMaterialState {
	MATERIAL material{};
	UVMatrixBuffer uv{};
	ID3D11ShaderResourceView* diffuseTexture = nullptr;
	std::uint32_t shaderID = 0;

	bool UsesDiffuseTexture() const noexcept {
		return (material.MaterialFlags &
			MATERIAL_FLAG_USE_DIFFUSE_TEXTURE) != 0;
	}
};

struct StaticBatchModelMaterialResolveResult {
	StaticBatchModelMaterialState state;
	StaticBatchModelMaterialRejectReason rejectReason =
		StaticBatchModelMaterialRejectReason::InvalidRepresentativePacket;

	bool IsEligible() const noexcept {
		return rejectReason == StaticBatchModelMaterialRejectReason::None;
	}
};

namespace StaticBatchModelMaterialResolver {

inline StaticBatchModelMaterialRejectReason ResolvePacketState(
	const RenderPacket& packet,
	StaticBatchModelMaterialState& state
) noexcept {
	const MaterialComponent* materialComponent = packet.bindings.material;
	const std::uint32_t expectedShaderID = materialComponent
		? static_cast<std::uint32_t>((std::max)(0, materialComponent->ShaderID))
		: 0u;
	if(packet.materialKey != expectedShaderID){
		return StaticBatchModelMaterialRejectReason::ShaderKeyMismatch;
	}
	if(materialComponent &&
		materialComponent->Material.BaseColor.w < 0.999f){
		return StaticBatchModelMaterialRejectReason::ExcludedByGBufferAlphaRule;
	}

	state = {};
	state.material.BaseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
	state.shaderID = expectedShaderID;
	state.uv = StaticBatchResourceKey::ResolveUVState(
		packet.bindings.texture
	);

	if(materialComponent){
		state.material = materialComponent->Material;
		state.material.MaterialFlags &=
			MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
	}

	const TextureComponent* textureComponent = packet.bindings.texture;
	if(textureComponent && textureComponent->m_TextureData){
		if(!textureComponent->m_TextureData->pTexture){
			return StaticBatchModelMaterialRejectReason::MissingOverrideDiffuseTexture;
		}
		state.diffuseTexture =
			textureComponent->m_TextureData->pTexture.Get();
		state.material.MaterialFlags |=
			MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
	}
	return StaticBatchModelMaterialRejectReason::None;
}

inline StaticBatchModelMaterialResolveResult Resolve(
	const StaticBatchPacketCacheEntry& group,
	std::span<const RenderPacket> packets
){
	StaticBatchModelMaterialResolveResult result;
	if(group.representativePacketIndex >= packets.size()){
		return result;
	}

	const RenderPacket& packet = packets[group.representativePacketIndex];
	if(packet.sceneContextID != group.sceneContextID ||
		packet.kind != group.key.kind ||
		packet.layer != group.key.layer){
		result.rejectReason =
			StaticBatchModelMaterialRejectReason::GroupPacketMismatch;
		return result;
	}
	if(packet.kind != RenderPacketKind::Model){
		result.rejectReason =
			StaticBatchModelMaterialRejectReason::UnsupportedPacketKind;
		return result;
	}
	if(packet.layer != RenderLayer::Opaque3D){
		result.rejectReason =
			StaticBatchModelMaterialRejectReason::UnsupportedRenderLayer;
		return result;
	}
	if(!HasRenderPacketPass(packet.passMask, RenderPacketPassMask::GBuffer)){
		result.rejectReason =
			StaticBatchModelMaterialRejectReason::MissingGBufferPass;
		return result;
	}

	const StaticBatchModelMaterialRejectReason packetStateResult =
		ResolvePacketState(packet, result.state);
	if(packetStateResult != StaticBatchModelMaterialRejectReason::None){
		result.rejectReason = packetStateResult;
		return result;
	}

	const ModelRendererComponent* renderer = packet.bindings.modelRenderer;
	if(!renderer){
		result.rejectReason =
			StaticBatchModelMaterialRejectReason::MissingModelRenderer;
		return result;
	}
	const std::shared_ptr<ModelData>& model = renderer->model;
	if(!model || !model->AiScene){
		result.rejectReason =
			StaticBatchModelMaterialRejectReason::MissingModelResource;
		return result;
	}
	if(model->AiScene->mNumMeshes != 1){
		result.rejectReason =
			StaticBatchModelMaterialRejectReason::UnsupportedSubMeshCount;
		return result;
	}
	if(!model->AiScene->mMeshes || !model->AiScene->mMeshes[0]){
		result.rejectReason =
			StaticBatchModelMaterialRejectReason::MissingSubMesh;
		return result;
	}

	const aiMesh* mesh = model->AiScene->mMeshes[0];
	if(!model->AiScene->mMaterials ||
		mesh->mMaterialIndex >= model->AiScene->mNumMaterials){
		result.rejectReason =
			StaticBatchModelMaterialRejectReason::InvalidMaterialIndex;
		return result;
	}
	const aiMaterial* sourceMaterial =
		model->AiScene->mMaterials[mesh->mMaterialIndex];
	if(!sourceMaterial){
		result.rejectReason =
			StaticBatchModelMaterialRejectReason::MissingMaterial;
		return result;
	}

	const MaterialComponent* materialComponent = packet.bindings.material;
	const bool hasOverrideTexture =
		packet.bindings.texture &&
		packet.bindings.texture->m_TextureData;
	if(!hasOverrideTexture){
		aiColor4D color;
		if(sourceMaterial->Get(
			AI_MATKEY_COLOR_DIFFUSE,
			color
		) == AI_SUCCESS){
			result.state.material.BaseColor = float4(
				color.r,
				color.g,
				color.b,
				color.a
			);
			if(materialComponent){
				result.state.material.BaseColor.x *=
					materialComponent->Material.BaseColor.x;
				result.state.material.BaseColor.y *=
					materialComponent->Material.BaseColor.y;
				result.state.material.BaseColor.z *=
					materialComponent->Material.BaseColor.z;
				result.state.material.BaseColor.w *=
					materialComponent->Material.BaseColor.w;
			}
		}

		aiString textureName;
		if(sourceMaterial->GetTexture(
			aiTextureType_DIFFUSE,
			0,
			&textureName
		) == AI_SUCCESS && textureName.length > 0){
			const auto diffuseIt =
				model->m_Texture.find(textureName.C_Str());
			if(diffuseIt != model->m_Texture.end()){
				if(!diffuseIt->second){
					result.rejectReason =
						StaticBatchModelMaterialRejectReason::MissingModelDiffuseTexture;
					return result;
				}
				result.state.diffuseTexture = diffuseIt->second;
				result.state.material.MaterialFlags |=
					MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
			}
		}

		aiString normalName;
		if(sourceMaterial->GetTexture(
			aiTextureType_NORMALS,
			0,
			&normalName
		) == AI_SUCCESS && normalName.length > 0){
			const auto normalIt = model->m_Texture.find(normalName.C_Str());
			if(normalIt != model->m_Texture.end()){
				result.rejectReason =
					StaticBatchModelMaterialRejectReason::NormalMapUnsupported;
				return result;
			}
		}
	}

	if(result.state.UsesDiffuseTexture() && !result.state.diffuseTexture){
		result.rejectReason =
			StaticBatchModelMaterialRejectReason::MissingModelDiffuseTexture;
		return result;
	}

	const StaticBatchResourceKeySet keys =
		StaticBatchResourceKey::Build(packet);
	if(keys.pipelineKey != group.key.pipelineKey ||
		keys.geometryKey != group.key.geometryKey ||
		keys.textureSetKey != group.key.textureSetKey ||
		keys.materialStateKey != group.key.materialStateKey){
		result.rejectReason =
			StaticBatchModelMaterialRejectReason::ResourceKeyMismatch;
		return result;
	}

	result.rejectReason = StaticBatchModelMaterialRejectReason::None;
	return result;
}

} // namespace StaticBatchModelMaterialResolver
