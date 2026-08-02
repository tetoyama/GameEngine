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
	float sliceX,
	float sliceY,
	int animationFrame
) noexcept {
	UVMatrixBuffer uv{};
	uv.UVStart = float2(0.0f, 0.0f);
	uv.UVEnd = float2(1.0f, 1.0f);
	if(sliceX <= 0.0f || sliceY <= 0.0f){
		return uv;
	}

	const bool isSliceX = TextureComponent::IsSliceValue(sliceX);
	const bool isSliceY = TextureComponent::IsSliceValue(sliceY);
	const float spanX = TextureComponent::ResolveUVSpan(sliceX);
	const float spanY = TextureComponent::ResolveUVSpan(sliceY);
	const int columnCount = TextureComponent::ResolveSliceCount(sliceX);
	const int rowCount = TextureComponent::ResolveSliceCount(sliceY);
	const int maxFrame = (std::max)(0, columnCount * rowCount - 1);
	const int safeFrame = std::clamp(animationFrame, 0, maxFrame);

	if(isSliceX){
		uv.UVStart.x = (safeFrame % columnCount) * spanX;
		uv.UVEnd.x = uv.UVStart.x + spanX;
	}else{
		uv.UVStart.x = 0.0f;
		uv.UVEnd.x = spanX;
	}

	if(isSliceY){
		uv.UVStart.y = (safeFrame / columnCount) * spanY;
		uv.UVEnd.y = uv.UVStart.y + spanY;
	}else{
		uv.UVStart.y = 0.0f;
		uv.UVEnd.y = spanY;
	}

	return uv;
}

inline UVMatrixBuffer ResolveUVState(
	const TextureComponent* texture
) noexcept {
	if(!texture){
		return ResolveUVState(1.0f, 1.0f, 0);
	}
	return ResolveUVState(
		texture->UV_Slice_X,
		texture->UV_Slice_Y,
		texture->AnimationNum
	);
}

inline std::uint64_t MakeUVStateKey(const UVMatrixBuffer& uv) noexcept {
	std::uint64_t key = 0x55565354415445ull;
	CombineFloat(key, uv.UVStart.x);
	CombineFloat(key, uv.UVStart.y);
	CombineFloat(key, uv.UVEnd.x);
	CombineFloat(key, uv.UVEnd.y);
	return key == 0 ? 1 : key;
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

inline void CombineMaterialDescriptor(
	std::uint64_t& hash,
	const MaterialDescriptor& descriptor
) noexcept {
	Combine(hash, static_cast<std::uint64_t>(
		static_cast<std::uint32_t>((std::max)(0, descriptor.shaderID))
	));
	for(float value : descriptor.parameters.baseColor){
		CombineFloat(hash, value);
	}
	for(float value : descriptor.parameters.emissiveColor){
		CombineFloat(hash, value);
	}
	CombineFloat(hash, descriptor.parameters.metallic);
	CombineFloat(hash, descriptor.parameters.roughness);
	CombineFloat(hash, descriptor.parameters.ambientOcclusion);
	CombineFloat(hash, descriptor.parameters.emissiveIntensity);
	CombineFloat(hash, descriptor.parameters.opacity);
	CombineFloat(hash, descriptor.parameters.normalScale);
	CombineFloat(hash, descriptor.parameters.heightScale);
	Combine(hash, static_cast<std::uint64_t>(descriptor.renderState.alphaMode));
	Combine(hash, static_cast<std::uint64_t>(descriptor.renderState.cullMode));
	CombineFloat(hash, descriptor.renderState.alphaCutoff);
	Combine(hash, descriptor.renderState.depthWrite ? 1ull : 0ull);
	Combine(hash, descriptor.renderState.receiveShadow ? 1ull : 0ull);
	Combine(hash, descriptor.legacyMaterialFlags);
}

inline std::uint64_t MakePipelineKey(const RenderPacket& packet) noexcept {
	std::uint64_t key = 0x504950454c494e45ull;
	Combine(key, static_cast<std::uint64_t>(packet.kind));
	Combine(key, static_cast<std::uint64_t>(packet.layer));
	Combine(key, static_cast<std::uint64_t>(packet.passMask));
	Combine(key, static_cast<std::uint64_t>(packet.materialKey));
	return key == 0 ? 1 : key;
}

inline void CombineMeshGeometry(
	std::uint64_t& key,
	const aiMesh* mesh,
	const ModelMeshGeometryCpuData* geometry
) noexcept {
	if(!mesh || !geometry || !geometry->IsValid()){
		Combine(key, 0);
		return;
	}
	Combine(key, static_cast<std::uint64_t>(geometry->vertices.size()));
	Combine(key, static_cast<std::uint64_t>(geometry->indices.size()));
	Combine(key, static_cast<std::uint64_t>(mesh->mMaterialIndex));
	Combine(key, mesh->HasBones() ? 1ull : 0ull);
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
	if(renderer.modelFilePath.empty() || !model || !model->AiScene ||
		!model->AiScene->mMeshes ||
		model->MeshGeometry.size() != model->AiScene->mNumMeshes){
		return 0;
	}

	std::uint64_t key = HashString(renderer.modelFilePath);
	Combine(key, renderer.modelRuntimeRevision);
	Combine(key, renderer.isBlender ? 1ull : 0ull);
	Combine(key, static_cast<std::uint64_t>(model->AiScene->mNumMeshes));
	Combine(key, static_cast<std::uint64_t>(model->MeshGeometry.size()));
	Combine(key, static_cast<std::uint64_t>(packet.subMeshIndex));
	Combine(key, static_cast<std::uint64_t>(packet.subMeshID));
	Combine(key, model->GetGeometryRevision());

	if(!packet.TargetsAllSubMeshes()){
		if(packet.subMeshIndex >= model->AiScene->mNumMeshes) return 0;
		CombineMeshGeometry(
			key,
			model->AiScene->mMeshes[packet.subMeshIndex],
			&model->MeshGeometry[packet.subMeshIndex]
		);
		return key == 0 ? 1 : key;
	}

	for(unsigned int meshIndex = 0;
		meshIndex < model->AiScene->mNumMeshes;
		++meshIndex){
		CombineMeshGeometry(
			key,
			model->AiScene->mMeshes[meshIndex],
			&model->MeshGeometry[meshIndex]
		);
	}
	return key == 0 ? 1 : key;
}

inline void CombineDescriptorTextureSet(
	std::uint64_t& key,
	const ModelData& model,
	const MaterialDescriptor& descriptor
) noexcept {
	Combine(key, static_cast<std::uint64_t>(descriptor.textures.size()));
	for(const MaterialTextureBinding& binding : descriptor.textures){
		Combine(key, static_cast<std::uint64_t>(binding.semantic));
		Combine(key, static_cast<std::uint64_t>(binding.colorSpace));
		Combine(key, HashString(binding.assetPath));
		Combine(key, HashString(binding.sourcePath));
		Combine(key, binding.sourceTextureIndex);
		Combine(key, binding.uvChannel);
		CombineFloat(key, binding.uvScale[0]);
		CombineFloat(key, binding.uvScale[1]);
		CombineFloat(key, binding.uvOffset[0]);
		CombineFloat(key, binding.uvOffset[1]);
		CombineFloat(key, binding.uvRotation);
		CombineFloat(key, binding.strength);
		Combine(key, binding.embedded ? 1ull : 0ull);

		auto found = binding.assetPath.empty()
			? model.m_Texture.end()
			: model.m_Texture.find(binding.assetPath);
		if(found == model.m_Texture.end() && !binding.sourcePath.empty()){
			found = model.m_Texture.find(binding.sourcePath);
		}
		Combine(key, found != model.m_Texture.end() ? 1ull : 0ull);
		Combine(
			key,
			found != model.m_Texture.end() && found->second != nullptr
				? 1ull
				: 0ull
		);
	}
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
		Combine(key, static_cast<std::uint64_t>(packet.subMeshIndex));
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

	const std::shared_ptr<ModelData> model = packet.modelResource
		? packet.modelResource
		: packet.bindings.modelRenderer->model;
	if(!model || !model->AiScene) return 0;
	if(!packet.TargetsAllSubMeshes() &&
		packet.subMeshIndex >= model->AiScene->mNumMeshes){
		return 0;
	}

	if(const MaterialDescriptor* descriptor =
		packet.modelMaterial.GetDescriptor()){
		std::uint64_t key = 0x4d4f44454c444553ull;
		Combine(key, static_cast<std::uint64_t>(packet.subMeshIndex));
		Combine(key, static_cast<std::uint64_t>(packet.subMeshID));
		CombineDescriptorTextureSet(key, *model, *descriptor);
		return key == 0 ? 1 : key;
	}

	std::vector<std::string_view> textureNames;
	textureNames.reserve(model->m_Texture.size());
	for(const auto& [name, texture] : model->m_Texture){
		(void)texture;
		textureNames.emplace_back(name);
	}
	std::sort(textureNames.begin(), textureNames.end());

	std::uint64_t key = 0x4d4f44454c544558ull;
	Combine(key, static_cast<std::uint64_t>(packet.subMeshIndex));
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
	const RenderPacket& packet,
	const ModelRendererComponent& renderer
) noexcept {
	const std::shared_ptr<ModelData>& model = renderer.model;
	if(renderer.modelFilePath.empty() || !model || !model->AiScene ||
		!model->AiScene->mMeshes){
		return 0;
	}

	std::uint64_t key = HashString(renderer.modelFilePath);
	Combine(key, renderer.modelRuntimeRevision);
	Combine(key, static_cast<std::uint64_t>(model->AiScene->mNumMaterials));
	Combine(key, static_cast<std::uint64_t>(packet.subMeshIndex));

	if(!packet.TargetsAllSubMeshes()){
		if(packet.subMeshIndex >= model->AiScene->mNumMeshes) return 0;
		const aiMesh* mesh = model->AiScene->mMeshes[packet.subMeshIndex];
		Combine(
			key,
			mesh ? static_cast<std::uint64_t>(mesh->mMaterialIndex) : 0ull
		);
		return key == 0 ? 1 : key;
	}

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
	const MaterialDescriptor* resolvedDescriptor =
		packet.modelMaterial.GetDescriptor();
	if(resolvedDescriptor){
		CombineMaterialDescriptor(key, *resolvedDescriptor);
		Combine(key, static_cast<std::uint64_t>(packet.modelMaterial.source));
		Combine(key, packet.modelMaterial.importedMaterialID);
		Combine(key, packet.modelMaterial.customMaterialID);
		Combine(key, packet.modelMaterial.usedFallback ? 1ull : 0ull);
	}else if(const MaterialComponent* component = packet.bindings.material){
		Combine(key, static_cast<std::uint64_t>(component->ShaderID));
		CombineMaterial(key, component->Material);
	}else if(packet.kind == RenderPacketKind::Mesh){
		Combine(key, 1);
	}else if(packet.kind != RenderPacketKind::Model){
		return 0;
	}

	if(packet.kind == RenderPacketKind::Model && !resolvedDescriptor){
		if(!packet.bindings.modelRenderer) return 0;
		const std::uint64_t modelMaterialKey =
			MakeModelMaterialStateKey(packet, *packet.bindings.modelRenderer);
		if(modelMaterialKey == 0) return 0;
		Combine(key, modelMaterialKey);
	}

	const UVMatrixBuffer uv = ResolveUVState(packet.bindings.texture);
	Combine(key, MakeUVStateKey(uv));
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
