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
		return HashString(packet.bindings.modelRenderer->modelFilePath);
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

	return 0;
}

inline std::uint64_t MakeMaterialStateKey(const RenderPacket& packet) noexcept {
	const MaterialComponent* component = packet.bindings.material;
	if(!component) return 1;

	const MATERIAL& material = component->Material;
	std::uint64_t key = 0x4d4154455249414cull;
	Combine(key, static_cast<std::uint64_t>(component->ShaderID));
	CombineFloat(key, material.BaseColor.x);
	CombineFloat(key, material.BaseColor.y);
	CombineFloat(key, material.BaseColor.z);
	CombineFloat(key, material.BaseColor.w);
	CombineFloat(key, material.Metallic);
	CombineFloat(key, material.Roughness);
	CombineFloat(key, material.AO);
	CombineFloat(key, material.EmissiveColor.x);
	CombineFloat(key, material.EmissiveColor.y);
	CombineFloat(key, material.EmissiveColor.z);
	CombineFloat(key, material.EmissiveIntensity);
	Combine(key, static_cast<std::uint64_t>(material.MaterialFlags));
	return key == 0 ? 1 : key;
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
