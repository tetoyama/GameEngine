#include <cassert>
#include <memory>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchModelMaterialResolver.h"

int main(){
	constexpr StaticBatchModelMaterialResolvePolicy gBufferPolicy =
		StaticBatchModelMaterialResolvePolicy::GBuffer();
	constexpr StaticBatchModelMaterialResolvePolicy shadowPolicy =
		StaticBatchModelMaterialResolvePolicy::Shadow();
	static_assert(gBufferPolicy.requireGBufferPass);
	static_assert(gBufferPolicy.applyGBufferAlphaRule);
	static_assert(gBufferPolicy.rejectNormalMapReference);
	static_assert(!shadowPolicy.requireGBufferPass);
	static_assert(!shadowPolicy.applyGBufferAlphaRule);
	static_assert(!shadowPolicy.rejectNormalMapReference);

	MATERIAL material{};
	material.BaseColor = float4(0.25f, 0.5f, 0.75f, 1.0f);
	material.Metallic = 0.3f;
	material.Roughness = 0.6f;
	material.AO = 0.9f;
	material.MaterialFlags =
		MATERIAL_FLAG_USE_DIFFUSE_TEXTURE |
		MATERIAL_FLAG_USE_NORMAL_TEXTURE |
		MATERIAL_FLAG_USE_ENVIRONMENT_MAP;

	StaticBatchModelMaterialInput input;
	input.material = &material;
	input.shaderID = 2;
	input.uv = StaticBatchResourceKey::ResolveUVState(4.0f, 2.0f, 3);

	StaticBatchModelMaterialState state;
	auto result = StaticBatchModelPacketMaterial::Resolve(
		2,
		input,
		state
	);
	assert(result == StaticBatchModelMaterialRejectReason::None);
	assert(state.shaderID == 2);
	assert(state.material.BaseColor.x == 0.25f);
	assert(state.material.BaseColor.y == 0.5f);
	assert(state.material.BaseColor.z == 0.75f);
	assert(state.material.BaseColor.w == 1.0f);
	assert(state.material.Metallic == 0.3f);
	assert(state.material.Roughness == 0.6f);
	assert(state.material.AO == 0.9f);
	assert(
		state.material.MaterialFlags ==
		MATERIAL_FLAG_USE_ENVIRONMENT_MAP
	);
	assert(!state.UsesDiffuseTexture());
	assert(state.diffuseTexture == nullptr);
	assert(state.uv.UVStart.x == 0.75f);
	assert(state.uv.UVStart.y == 0.0f);
	assert(state.uv.UVEnd.x == 1.0f);
	assert(state.uv.UVEnd.y == 0.5f);

	result = StaticBatchModelPacketMaterial::Resolve(
		3,
		input,
		state
	);
	assert(result == StaticBatchModelMaterialRejectReason::ShaderKeyMismatch);

	material.BaseColor.w = 0.5f;
	result = StaticBatchModelPacketMaterial::Resolve(
		2,
		input,
		state
	);
	assert(
		result ==
		StaticBatchModelMaterialRejectReason::ExcludedByGBufferAlphaRule
	);

	result = StaticBatchModelPacketMaterial::Resolve(
		2,
		input,
		state,
		false
	);
	assert(result == StaticBatchModelMaterialRejectReason::None);
	assert(state.material.BaseColor.w == 0.5f);

	material.BaseColor.w = 1.0f;
	input.hasOverrideDiffuseTexture = true;
	input.overrideDiffuseTexture = nullptr;
	result = StaticBatchModelPacketMaterial::Resolve(
		2,
		input,
		state
	);
	assert(
		result ==
		StaticBatchModelMaterialRejectReason::MissingOverrideDiffuseTexture
	);

	// Snapshot path uses MaterialAlphaMode rather than legacy BaseColor alpha.
	// Blend may have alpha=1 when opacity comes from a texture, while Masked may
	// legitimately carry alpha<1 and still remain in the GBuffer path.
	RenderPacket packet;
	packet.kind = RenderPacketKind::Model;
	packet.layer = RenderLayer::Opaque3D;
	packet.passMask = RenderPacketPassMask::Shadow |
		RenderPacketPassMask::GBuffer;
	packet.materialKey = 2;

	MaterialDescriptor descriptor;
	descriptor.shaderID = 2;
	descriptor.renderState.alphaMode = MaterialAlphaMode::Blend;
	descriptor.parameters.baseColor[3] = 1.0f;
	packet.modelMaterial.ownedDescriptor =
		std::make_shared<MaterialDescriptor>(descriptor);
	packet.modelMaterial.descriptor =
		packet.modelMaterial.ownedDescriptor.get();

	result = StaticBatchModelPacketMaterial::Resolve(packet, state, true);
	assert(
		result ==
		StaticBatchModelMaterialRejectReason::ExcludedByGBufferAlphaRule
	);
	result = StaticBatchModelPacketMaterial::Resolve(packet, state, false);
	assert(result == StaticBatchModelMaterialRejectReason::None);

	descriptor.renderState.alphaMode = MaterialAlphaMode::Masked;
	descriptor.parameters.baseColor[3] = 0.25f;
	packet.modelMaterial.ownedDescriptor =
		std::make_shared<MaterialDescriptor>(descriptor);
	packet.modelMaterial.descriptor =
		packet.modelMaterial.ownedDescriptor.get();
	result = StaticBatchModelPacketMaterial::Resolve(packet, state, true);
	assert(result == StaticBatchModelMaterialRejectReason::None);
	assert(state.material.BaseColor.w == 0.25f);
	return 0;
}