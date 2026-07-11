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

struct StaticBatchModelMaterialInput {
	const MATERIAL* material = nullptr;
	int shaderID = 0;
	UVMatrixBuffer uv{};
	bool hasOverrideDiffuseTexture = false;
	ID3D11ShaderResourceView* overrideDiffuseTexture = nullptr;
};

namespace StaticBatchModelPacketMaterial {

inline StaticBatchModelMaterialRejectReason Resolve(
	std::uint32_t packetMaterialKey,
	const StaticBatchModelMaterialInput& input,
	StaticBatchModelMaterialState& state,
	bool applyGBufferAlphaRule = true
) noexcept {
	const std::uint32_t expectedShaderID =
		static_cast<std::uint32_t>((std::max)(0, input.shaderID));
	if(packetMaterialKey != expectedShaderID){
		return StaticBatchModelMaterialRejectReason::ShaderKeyMismatch;
	}
	if(applyGBufferAlphaRule && input.material &&
		input.material->BaseColor.w < 0.999f){
		return StaticBatchModelMaterialRejectReason::ExcludedByGBufferAlphaRule;
	}

	state = {};
	state.material.BaseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
	state.shaderID = expectedShaderID;
	state.uv = input.uv;

	if(input.material){
		state.material = *input.material;
		state.material.MaterialFlags &=
			MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
	}

	if(input.hasOverrideDiffuseTexture){
		if(!input.overrideDiffuseTexture){
			return StaticBatchModelMaterialRejectReason::MissingOverrideDiffuseTexture;
		}
		state.diffuseTexture = input.overrideDiffuseTexture;
		state.material.MaterialFlags |=
			MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
	}
	return StaticBatchModelMaterialRejectReason::None;
}

inline StaticBatchModelMaterialRejectReason Resolve(
	const RenderPacket& packet,
	StaticBatchModelMaterialState& state,
	bool applyGBufferAlphaRule = true
) noexcept {
	const MaterialComponent* materialComponent = packet.bindings.material;
	const TextureComponent* textureComponent = packet.bindings.texture;
	const bool hasOverrideTexture =
		textureComponent && textureComponent->m_TextureData;
	ID3D11ShaderResourceView* overrideTexture = nullptr;
	if(hasOverrideTexture && textureComponent->m_TextureData->pTexture){
		overrideTexture = textureComponent->m_TextureData->pTexture.Get();
	}

	const StaticBatchModelMaterialInput input{
		materialComponent ? &materialComponent->Material : nullptr,
		materialComponent ? materialComponent->ShaderID : 0,
		StaticBatchResourceKey::ResolveUVState(textureComponent),
		hasOverrideTexture,
		overrideTexture
	};
	return Resolve(
		packet.materialKey,
		input,
		state,
		applyGBufferAlphaRule
	);
}

} // namespace StaticBatchModelPacketMaterial
