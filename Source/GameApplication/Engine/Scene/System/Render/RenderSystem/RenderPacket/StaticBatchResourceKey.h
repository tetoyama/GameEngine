#pragma once

#include <cstdint>
#include <string_view>

#include "RenderPacket.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Component/textureComponent.h"

struct StaticBatchResourceKeySet {
	std::uint64_t pipelineKey = 0;
	std::uint64_t geometryKey = 0;
	std::uint64_t textureSetKey = 0;

	bool IsComplete() const noexcept {
		return pipelineKey != 0 && geometryKey != 0 && textureSetKey != 0;
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
		// Meshは外部Textureを持たない状態を明示的に識別できる。
		return 1;
	}

	// Model内部Material TextureはまだAsset Keyへ展開されていない。
	return 0;
}

inline StaticBatchResourceKeySet Build(const RenderPacket& packet) noexcept {
	return {
		MakePipelineKey(packet),
		MakeGeometryKey(packet),
		MakeTextureSetKey(packet)
	};
}

} // namespace StaticBatchResourceKey
