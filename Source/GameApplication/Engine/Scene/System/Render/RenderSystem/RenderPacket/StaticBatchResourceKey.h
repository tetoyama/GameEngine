#pragma once

#include <bit>
#include <cstdint>
#include <string_view>

#include "RenderPacket.h"
#include "Scene/Component/materialComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Component/textureComponent.h"

struct StaticBatchResourceKeySet {
	std::uint64_t pipelineKey = 0;
	std::uint64_t geometryKey = 0;
	std::uint64_t textureSetKey = 0;
	std::uint64_t materialStateKey = 0;

	bool IsComplete() const noexcept {
		return pipelineKey != 0 &&
			geometryKey != 0 &&
			textureSetKey != 0 &&
			materialStateKey != 0;
	}
};

namespace StaticBatchResourceKey {

inline std::uint64_t HashString(std::string_view text) noexcept {
	if(text.empty()) return 0;
	std::uint64_t hash = 1469598103934665603ull;
	for(unsigned char value : text){
		hash ^= static_cast<std::uint64_t>(value);
		hash *= 1099511628211ull;
	}
	return hash == 0 ? 1 : hash;
}

inline void Combine(std::uint64_t& hash, std::uint64_t value) noexcept {
	hash ^= value + 0x9e3779b97f4a7c15ull +
		(hash << 6) + (hash >> 2);
}

inline void CombineFloat(std::uint64_t& hash, float value) noexcept {
	Combine(hash, std::bit_cast<std::uint32_t>(value));
}

inline void CombineMaterial(
	std::uint64_t& hash,
	const MATERIAL& material
) noexcept {
	CombineFloat(hash, material.BaseColor.x);
	CombineFloat(hash, material.BaseColor.y);
	CombineFloat(hash, material.BaseColor.z);
	CombineFloat(hash, material.BaseColor.w);
	CombineFloat(hash, material.Metallic);
	CombineFloat(hash, material.Roughness);
	CombineFloat(hash, material.AO);
	CombineFloat(hash, material.EmissiveColor.x);
	CombineFloat(hash, material.EmissiveColor.y);
	CombineFloat(hash, material.EmissiveColor.z);
	CombineFloat(hash, material.EmissiveIntensity);
	Combine(hash, static_cast<std::uint64_t>(material.MaterialFlags));
}

inline std::uint64_t MakePipelineKey(const RenderPacket& packet) noexcept {
	std::uint64_t key = 0x504950454c494e45ull;
	Combine(key, static_cast<std::uint64_t>(packet.kind));
	Combine(key, static_cast<std::uint64_t>(packet.layer));
	Combine(key, static_cast<std::uint64_t>(packet.passMask));
	Combine(key, static_cast<std::uint64_t>(packet.materialKey));
	return key == 0 ? 1 : key;
}

inline std::uint64_t MakeGeometryKey(const RenderPacket& packet) noexcept {
	if(packet.kind == RenderPacketKind::Mesh && packet.bindings.meshRenderer){
		return packet.bindings.meshRenderer->mesh.geometryResourceKey;
	}

	if(packet.kind == RenderPacketKind::Model && packet.bindings.modelRenderer){
		const ModelRendererComponent& renderer = *packet.bindings.modelRenderer;
		std::uint64_t key = HashString(renderer.modelFilePath);
		if(key == 0) return 0;

		Combine(key, renderer.modelRuntimeRevision);
		if(renderer.model){
			Combine(key, static_cast<std::uint64_t>(renderer.model->m_MeshData.size()));
			Combine(key, static_cast<std::uint64_t>(renderer.model->m_SourceVertices.size()));
			Combine(key, static_cast<std::uint64_t>(renderer.model->m_SourceIndices.size()));
			Combine(key, renderer.model->isBlender ? 1ull : 0ull);
		}
		return key == 0 ? 1 : key;
	}
	return 0;
}

inline std::uint64_t MakeTextureSetKey(const RenderPacket& packet) noexcept {
	if(packet.bindings.texture){
		const std::shared_ptr<TextureData>& texture =
			packet.bindings.texture->m_TextureData;
		return texture ? HashString(texture->FilePath) : 0;
	}

	if(packet.kind == RenderPacketKind::Mesh && packet.bindings.meshRenderer){
		const TextureData* texture =
			packet.bindings.meshRenderer->mesh.m_TextureData;
		if(texture) return HashString(texture->FilePath);
		return 1;
	}

	if(packet.kind == RenderPacketKind::Model && packet.bindings.modelRenderer){
		const std::shared_ptr<ModelData>& model =
			packet.bindings.modelRenderer->model;
		if(!model) return 0;

		std::uint64_t key = 0x4d4f44454c544558ull;
		Combine(key, static_cast<std::uint64_t>(model->m_Textures.size()));
		for(const std::shared_ptr<TextureData>& texture : model->m_Textures){
			Combine(key, texture ? HashString(texture->FilePath) : 0ull);
		}
		Combine(key, static_cast<std::uint64_t>(model->m_TexSlot.size()));
		for(unsigned int slot : model->m_TexSlot){
			Combine(key, static_cast<std::uint64_t>(slot));
		}
		// Textureなしも明示的なTexture Setとして一意に扱う。
		return key == 0 ? 1 : key;
	}

	return 0;
}

inline std::uint64_t MakeMaterialStateKey(const RenderPacket& packet) noexcept {
	if(const MaterialComponent* component = packet.bindings.material){
		std::uint64_t key = 0x4d4154455249414cull;
		Combine(key, static_cast<std::uint64_t>(component->ShaderID));
		CombineMaterial(key, component->Material);
		return key == 0 ? 1 : key;
	}

	if(packet.kind == RenderPacketKind::Model && packet.bindings.modelRenderer){
		const std::shared_ptr<ModelData>& model =
			packet.bindings.modelRenderer->model;
		if(!model) return 0;

		std::uint64_t key = 0x4d4f44454c4d4154ull;
		Combine(key, static_cast<std::uint64_t>(model->m_Materials.size()));
		for(const MATERIAL& material : model->m_Materials){
			CombineMaterial(key, material);
		}
		return key == 0 ? 1 : key;
	}

	// MaterialComponentなしのMeshは既定Materialとして一意に扱う。
	return packet.kind == RenderPacketKind::Mesh ? 1 : 0;
}

inline StaticBatchResourceKeySet Build(const RenderPacket& packet) noexcept {
	return {
		MakePipelineKey(packet),
		MakeGeometryKey(packet),
		MakeTextureSetKey(packet),
		MakeMaterialStateKey(packet)
	};
}

} // namespace StaticBatchResourceKey
