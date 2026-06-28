#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

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
	for(const char value : text){
		hash ^= static_cast<std::uint64_t>(
			static_cast<unsigned char>(value)
		);
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

inline UVMatrixBuffer ResolveUVState(
	const TextureComponent* texture
) noexcept {
	UVMatrixBuffer uv{};
	uv.UVStart = float2(0.0f, 0.0f);
	uv.UVEnd = float2(1.0f, 1.0f);
	if(!texture || texture->UV_Slice_X <= 0.0f ||
		texture->UV_Slice_Y <= 0.0f){
		return uv;
	}

	const int column = (std::max)(
		1,
		static_cast<int>(1.0f / texture->UV_Slice_X)
	);
	uv.UVStart.x =
		(texture->AnimationNum % column) * texture->UV_Slice_X;
	uv.UVStart.y =
		(texture->AnimationNum / column) * texture->UV_Slice_Y;
	uv.UVEnd.x = uv.UVStart.x + texture->UV_Slice_X;
	uv.UVEnd.y = uv.UVStart.y + texture->UV_Slice_Y;
	return uv;
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

	if(packet.kind != RenderPacketKind::Model ||
		!packet.bindings.modelRenderer){
		return 0;
	}

	const ModelRendererComponent& renderer = *packet.bindings.modelRenderer;
	const std::shared_ptr<ModelData>& model = renderer.model;
	if(renderer.modelFilePath.empty() || !model || !model->AiScene){
		return 0;
	}

	std::uint64_t key = HashString(renderer.modelFilePath);
	Combine(key, renderer.modelRuntimeRevision);
	Combine(key, renderer.isBlender ? 1ull : 0ull);
	Combine(key, static_cast<std::uint64_t>(model->AiScene->mNumMeshes));
	Combine(key, static_cast<std::uint64_t>(model->VertexBuffer.size()));
	Combine(key, static_cast<std::uint64_t>(model->IndexBuffer.size()));

	for(unsigned int meshIndex = 0;
		meshIndex < model->AiScene->mNumMeshes;
		++meshIndex){
		const aiMesh* mesh = model->AiScene->mMeshes[meshIndex];
		if(!mesh){
			Combine(key, 0);
			continue;
		}
		Combine(key, static_cast<std::uint64_t>(mesh->mNumVertices));
		Combine(key, static_cast<std::uint64_t>(mesh->mNumFaces));
		Combine(key, static_cast<std::uint64_t>(mesh->mMaterialIndex));
		Combine(key, mesh->HasBones() ? 1ull : 0ull);
	}
	return key == 0 ? 1 : key;
}

inline std::uint64_t MakeTextureSetKey(const RenderPacket& packet) noexcept {
	if(packet.bindings.texture &&
		packet.bindings.texture->m_TextureData){
		const std::shared_ptr<TextureData>& texture =
			packet.bindings.texture->m_TextureData;
		const std::uint64_t pathKey = HashString(texture->FilePath);
		if(pathKey == 0) return 0;

		std::uint64_t key = 0x4f56455252494445ull;
		Combine(key, pathKey);
		Combine(key, texture->pTexture ? 1ull : 0ull);
		return key == 0 ? 1 : key;
	}

	if(packet.kind == RenderPacketKind::Mesh && packet.bindings.meshRenderer){
		const TextureData* texture =
			packet.bindings.meshRenderer->mesh.m_TextureData;
		if(!texture) return 1;
		const std::uint64_t pathKey = HashString(texture->FilePath);
		if(pathKey == 0) return 0;
		std::uint64_t key = 0x4d455348544558ull;
		Combine(key, pathKey);
		Combine(key, texture->pTexture ? 1ull : 0ull);
		return key == 0 ? 1 : key;
	}

	if(packet.kind != RenderPacketKind::Model ||
		!packet.bindings.modelRenderer){
		return 0;
	}

	const std::shared_ptr<ModelData>& model =
		packet.bindings.modelRenderer->model;
	if(!model || !model->AiScene) return 0;

	std::vector<std::string_view> textureNames;
	textureNames.reserve(model->m_Texture.size());
	for(const auto& [name, texture] : model->m_Texture){
		(void)texture;
		textureNames.emplace_back(name);
	}
	std::sort(textureNames.begin(), textureNames.end());

	std::uint64_t key = 0x4d4f44454c544558ull;
	Combine(key, static_cast<std::uint64_t>(textureNames.size()));
	for(const std::string_view name : textureNames){
		Combine(key, HashString(name));
		const auto found = model->m_Texture.find(std::string(name));
		Combine(
			key,
			found != model->m_Texture.end() && found->second != nullptr
				? 1ull
				: 0ull
		);
	}
	return key == 0 ? 1 : key;
}

inline std::uint64_t MakeModelMaterialStateKey(
	const ModelRendererComponent& renderer
) noexcept {
	const std::shared_ptr<ModelData>& model = renderer.model;
	if(renderer.modelFilePath.empty() || !model || !model->AiScene){
		return 0;
	}

	std::uint64_t key = HashString(renderer.modelFilePath);
	Combine(key, renderer.modelRuntimeRevision);
	Combine(key, static_cast<std::uint64_t>(model->AiScene->mNumMaterials));
	for(unsigned int meshIndex = 0;
		meshIndex < model->AiScene->mNumMeshes;
		++meshIndex){
		const aiMesh* mesh = model->AiScene->mMeshes[meshIndex];
		Combine(
			key,
			mesh ? static_cast<std::uint64_t>(mesh->mMaterialIndex) : 0ull
		);
	}
	return key == 0 ? 1 : key;
}

inline std::uint64_t MakeMaterialStateKey(const RenderPacket& packet) noexcept {
	std::uint64_t key = 0x4d4154455249414cull;
	if(const MaterialComponent* component = packet.bindings.material){
		Combine(key, static_cast<std::uint64_t>(component->ShaderID));
		CombineMaterial(key, component->Material);
	}else if(packet.kind == RenderPacketKind::Mesh){
		Combine(key, 1);
	}else if(packet.kind != RenderPacketKind::Model){
		return 0;
	}

	if(packet.kind == RenderPacketKind::Model){
		if(!packet.bindings.modelRenderer) return 0;
		const std::uint64_t modelMaterialKey =
			MakeModelMaterialStateKey(*packet.bindings.modelRenderer);
		if(modelMaterialKey == 0) return 0;
		Combine(key, modelMaterialKey);
	}

	const UVMatrixBuffer uv = ResolveUVState(packet.bindings.texture);
	CombineFloat(key, uv.UVStart.x);
	CombineFloat(key, uv.UVStart.y);
	CombineFloat(key, uv.UVEnd.x);
	CombineFloat(key, uv.UVEnd.y);
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
