#include <cassert>

#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/StaticBatchResourceKey.h"

int main(){
	MeshRendererComponent mesh;
	mesh.mesh.SetGeometryResourceKey(0x1234u);

	TextureData meshTexture;
	meshTexture.FilePath = "Asset/Texture/ModelDiffuse.png";
	mesh.mesh.m_TextureData = &meshTexture;

	TextureComponent texture;
	texture.m_TextureData.reset();
	texture.UV_Slice_X = 1.0f;
	texture.UV_Slice_Y = 1.0f;
	texture.AnimationNum = 0;

	RenderPacket packet;
	packet.kind = RenderPacketKind::Mesh;
	packet.layer = RenderLayer::Transparent3D;
	packet.passMask = RenderPacketPassMask::Forward;
	packet.bindings.meshRenderer = &mesh;
	packet.bindings.texture = &texture;

	const StaticBatchResourceKeySet base =
		StaticBatchResourceKey::Build(packet);
	assert(base.IsComplete());
	assert(base.textureSetKey != 0);

	texture.UV_Slice_X = 0.25f;
	texture.UV_Slice_Y = 0.5f;
	const StaticBatchResourceKeySet sliced =
		StaticBatchResourceKey::Build(packet);
	assert(sliced.textureSetKey == base.textureSetKey);
	assert(sliced.materialStateKey != base.materialStateKey);

	texture.AnimationNum = 3;
	const StaticBatchResourceKeySet animated =
		StaticBatchResourceKey::Build(packet);
	assert(animated.textureSetKey == sliced.textureSetKey);
	assert(animated.materialStateKey != sliced.materialStateKey);

	const UVMatrixBuffer uv =
		StaticBatchResourceKey::ResolveUVState(&texture);
	assert(uv.UVStart.x == 0.75f);
	assert(uv.UVStart.y == 0.0f);
	assert(uv.UVEnd.x == 1.0f);
	assert(uv.UVEnd.y == 0.5f);

	texture.m_TextureData = std::make_shared<TextureData>();
	texture.m_TextureData->FilePath = "Asset/Texture/Override.png";
	const StaticBatchResourceKeySet overridden =
		StaticBatchResourceKey::Build(packet);
	assert(overridden.IsComplete());
	assert(overridden.textureSetKey != animated.textureSetKey);

	texture.m_TextureData.reset();
	const StaticBatchResourceKeySet fallback =
		StaticBatchResourceKey::Build(packet);
	assert(fallback.textureSetKey == animated.textureSetKey);
	return 0;
}
