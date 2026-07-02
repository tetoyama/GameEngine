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

	MaterialComponent material;
	material.ShaderID = 2;
	material.Material.BaseColor = float4(0.25f, 0.5f, 0.75f, 1.0f);
	material.Material.Metallic = 0.3f;
	material.Material.Roughness = 0.6f;
	material.Material.AO = 0.9f;
	material.Material.MaterialFlags =
		MATERIAL_FLAG_USE_DIFFUSE_TEXTURE |
		MATERIAL_FLAG_USE_NORMAL_TEXTURE |
		MATERIAL_FLAG_USE_ENVIRONMENT_MAP;

	TextureComponent texture;
	texture.UV_Slice_X = 0.25f;
	texture.UV_Slice_Y = 0.5f;
	texture.AnimationNum = 3;

	RenderPacket packet;
	packet.kind = RenderPacketKind::Model;
	packet.layer = RenderLayer::Opaque3D;
	packet.passMask = RenderPacketPassMask::GBuffer;
	packet.materialKey = 2;
	packet.bindings.material = &material;
	packet.bindings.texture = &texture;

	StaticBatchModelMaterialState state;
	auto result = StaticBatchModelPacketMaterial::Resolve(
		packet,
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

	packet.materialKey = 3;
	result = StaticBatchModelPacketMaterial::Resolve(
		packet,
		state
	);
	assert(result == StaticBatchModelMaterialRejectReason::ShaderKeyMismatch);

	packet.materialKey = 2;
	material.Material.BaseColor.w = 0.5f;
	result = StaticBatchModelPacketMaterial::Resolve(
		packet,
		state
	);
	assert(
		result ==
		StaticBatchModelMaterialRejectReason::ExcludedByGBufferAlphaRule
	);

	result = StaticBatchModelPacketMaterial::Resolve(
		packet,
		state,
		false
	);
	assert(result == StaticBatchModelMaterialRejectReason::None);
	assert(state.material.BaseColor.w == 0.5f);

	material.Material.BaseColor.w = 1.0f;
	texture.m_TextureData = std::make_shared<TextureData>();
	texture.m_TextureData->FilePath = "Asset/Texture/MissingGpuTexture.png";
	result = StaticBatchModelPacketMaterial::Resolve(
		packet,
		state
	);
	assert(
		result ==
		StaticBatchModelMaterialRejectReason::MissingOverrideDiffuseTexture
	);
	return 0;
}
