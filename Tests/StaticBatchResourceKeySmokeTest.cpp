#include <cassert>
#include <cstdint>

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
