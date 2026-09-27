#include <cassert>
#include <cstdint>
#include <memory>

#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/StaticBatchResourceKey.h"

int main(){
	const std::uint64_t textureA =
		StaticBatchResourceKey::HashString("Asset/Texture/A.png");
	const std::uint64_t textureB =
		StaticBatchResourceKey::HashString("Asset/Texture/B.png");
	assert(textureA != 0);
	assert(textureB != 0);
	assert(textureA != textureB);
	assert(StaticBatchResourceKey::HashString("") == 0);

	MATERIAL material{};
	material.BaseColor = {0.25f, 0.5f, 0.75f, 1.0f};
	material.Metallic = 0.2f;
	material.Roughness = 0.6f;
	material.AO = 0.9f;

	std::uint64_t materialKey = 0x4d4154455249414cull;
	StaticBatchResourceKey::CombineMaterial(materialKey, material);
	std::uint64_t identicalMaterialKey = 0x4d4154455249414cull;
	StaticBatchResourceKey::CombineMaterial(identicalMaterialKey, material);
	assert(materialKey == identicalMaterialKey);

	material.Roughness = 0.25f;
	std::uint64_t changedMaterialKey = 0x4d4154455249414cull;
	StaticBatchResourceKey::CombineMaterial(changedMaterialKey, material);
	assert(changedMaterialKey != materialKey);

	MaterialDescriptor descriptor;
	descriptor.shaderID = 4;
	descriptor.parameters.baseColor = {0.8f, 0.6f, 0.4f, 1.0f};
	descriptor.parameters.roughness = 0.35f;
	std::uint64_t descriptorKey = 0x4d415444455343ull;
	StaticBatchResourceKey::CombineMaterialDescriptor(
		descriptorKey,
		descriptor
	);
	MaterialDescriptor changedDescriptor = descriptor;
	changedDescriptor.parameters.roughness = 0.8f;
	std::uint64_t changedDescriptorKey = 0x4d415444455343ull;
	StaticBatchResourceKey::CombineMaterialDescriptor(
		changedDescriptorKey,
		changedDescriptor
	);
	assert(descriptorKey != changedDescriptorKey);

	RenderPacket packet;
	packet.kind = RenderPacketKind::Mesh;
	packet.layer = RenderLayer::Opaque3D;
	packet.passMask = RenderPacketPassMask::GBuffer;
	packet.materialKey = 2;
	const std::uint64_t pipelineKey =
		StaticBatchResourceKey::MakePipelineKey(packet);
	assert(pipelineKey != 0);
	assert(
		StaticBatchResourceKey::MakePipelineKey(packet) == pipelineKey
	);

	packet.materialKey = 3;
	assert(
		StaticBatchResourceKey::MakePipelineKey(packet) != pipelineKey
	);

	// Resolved Model MaterialはModelRenderer / aiMaterialを再探索せず、Packet
	// SnapshotだけからMaterial State Keyを生成する。
	packet.kind = RenderPacketKind::Model;
	packet.materialKey = 4;
	packet.modelMaterial.ownedDescriptor =
		std::make_shared<MaterialDescriptor>(descriptor);
	packet.modelMaterial.descriptor =
		packet.modelMaterial.ownedDescriptor.get();
	packet.modelMaterial.source = ModelMaterialResolutionSource::CustomMaterial;
	packet.modelMaterial.customMaterialID = 71;
	const std::uint64_t resolvedStateKey =
		StaticBatchResourceKey::MakeMaterialStateKey(packet);
	assert(resolvedStateKey != 0);

	packet.modelMaterial.ownedDescriptor =
		std::make_shared<MaterialDescriptor>(changedDescriptor);
	packet.modelMaterial.descriptor =
		packet.modelMaterial.ownedDescriptor.get();
	const std::uint64_t changedResolvedStateKey =
		StaticBatchResourceKey::MakeMaterialStateKey(packet);
	assert(changedResolvedStateKey != 0);
	assert(changedResolvedStateKey != resolvedStateKey);

	StaticBatchResourceKeySet complete{
		pipelineKey,
		0x1234u,
		textureA,
		materialKey
	};
	assert(complete.IsComplete());
	complete.geometryKey = 0;
	assert(!complete.IsComplete());
	return 0;
}