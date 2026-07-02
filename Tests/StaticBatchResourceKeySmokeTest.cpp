#include <cassert>
#include <memory>

#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/StaticBatchResourceKey.h"

int main(){
	MeshRendererComponent mesh;
	mesh.mesh.SetGeometryResourceKey(0x1234u);

	MaterialComponent material;
	material.ShaderID = 2;
	material.Material.BaseColor = {0.25f, 0.5f, 0.75f, 1.0f};
	material.Material.Metallic = 0.2f;
	material.Material.Roughness = 0.6f;
	material.Material.AO = 0.9f;

	TextureComponent texture;
	texture.m_TextureData = std::make_shared<TextureData>();
	texture.m_TextureData->FilePath = "Asset/Texture/A.png";

	RenderPacket packet;
	packet.kind = RenderPacketKind::Mesh;
	packet.layer = RenderLayer::Opaque3D;
	packet.passMask = RenderPacketPassMask::GBuffer;
	packet.materialKey = static_cast<std::uint32_t>(material.ShaderID);
	packet.bindings.meshRenderer = &mesh;
	packet.bindings.material = &material;
	packet.bindings.texture = &texture;

	const StaticBatchResourceKeySet original =
		StaticBatchResourceKey::Build(packet);
	assert(original.IsComplete());
	assert(original.geometryKey == mesh.mesh.geometryResourceKey);

	const StaticBatchResourceKeySet identical =
		StaticBatchResourceKey::Build(packet);
	assert(identical.pipelineKey == original.pipelineKey);
	assert(identical.geometryKey == original.geometryKey);
	assert(identical.textureSetKey == original.textureSetKey);
	assert(identical.materialStateKey == original.materialStateKey);

	material.Material.Roughness = 0.25f;
	const StaticBatchResourceKeySet changedMaterial =
		StaticBatchResourceKey::Build(packet);
	assert(changedMaterial.pipelineKey == original.pipelineKey);
	assert(changedMaterial.geometryKey == original.geometryKey);
	assert(changedMaterial.textureSetKey == original.textureSetKey);
	assert(changedMaterial.materialStateKey != original.materialStateKey);

	texture.m_TextureData->FilePath = "Asset/Texture/B.png";
	const StaticBatchResourceKeySet changedTexture =
		StaticBatchResourceKey::Build(packet);
	assert(changedTexture.textureSetKey != original.textureSetKey);
	assert(changedTexture.geometryKey == original.geometryKey);

	mesh.mesh.SetGeometryResourceKey(0x5678u);
	const StaticBatchResourceKeySet changedGeometry =
		StaticBatchResourceKey::Build(packet);
	assert(changedGeometry.geometryKey != original.geometryKey);

	ModelRendererComponent unloadedModel;
	unloadedModel.modelFilePath = "Asset/Model/Test.fbx";
	RenderPacket modelPacket;
	modelPacket.kind = RenderPacketKind::Model;
	modelPacket.layer = RenderLayer::Opaque3D;
	modelPacket.passMask = RenderPacketPassMask::GBuffer;
	modelPacket.bindings.modelRenderer = &unloadedModel;
	modelPacket.bindings.material = &material;

	const StaticBatchResourceKeySet unloadedModelKeys =
		StaticBatchResourceKey::Build(modelPacket);
	assert(unloadedModelKeys.pipelineKey != 0);
	assert(unloadedModelKeys.geometryKey == 0);
	assert(unloadedModelKeys.textureSetKey == 0);
	assert(unloadedModelKeys.materialStateKey == 0);
	assert(!unloadedModelKeys.IsComplete());
	return 0;
}
