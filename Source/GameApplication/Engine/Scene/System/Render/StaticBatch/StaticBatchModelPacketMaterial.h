#pragma once

#include <algorithm>
#include <cstdint>

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

namespace StaticBatchModelPacketMaterial {

inline StaticBatchModelMaterialRejectReason Resolve(
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

} // namespace StaticBatchModelPacketMaterial
