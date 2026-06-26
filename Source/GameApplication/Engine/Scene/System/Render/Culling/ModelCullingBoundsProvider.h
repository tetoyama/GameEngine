// =======================================================================
//
// ModelCullingBoundsProvider.h
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

#include "Scene/Component/CullingComponent.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Registry/componentRegistry.h"

namespace ModelCullingBoundsProvider {

inline Vector3 ToRenderPosition(
	const aiVector3D& source,
	bool isBlender
) noexcept {
	return isBlender
		? Vector3(source.x, -source.z, source.y)
		: Vector3(source.x, source.y, source.z);
}

inline void ExpandBounds(
	EntityAABB& bounds,
	bool& hasVertex,
	const Vector3& position
) noexcept {
	if(!hasVertex){
		bounds.min = position;
		bounds.max = position;
		hasVertex = true;
		return;
	}

	bounds.min.x = (std::min)(bounds.min.x, position.x);
	bounds.min.y = (std::min)(bounds.min.y, position.y);
	bounds.min.z = (std::min)(bounds.min.z, position.z);
	bounds.max.x = (std::max)(bounds.max.x, position.x);
	bounds.max.y = (std::max)(bounds.max.y, position.y);
	bounds.max.z = (std::max)(bounds.max.z, position.z);
}

// Assimp Scene所有権や外部ライブラリの寿命に依存しない純粋なBounds計算契約。
inline bool TryBuildLocalBoundsFromPositions(
	std::span<const aiVector3D> positions,
	bool isBlender,
	EntityAABB& outBounds
) noexcept {
	bool hasVertex = false;
	EntityAABB bounds{};
	for(const aiVector3D& source : positions){
		ExpandBounds(
			bounds,
			hasVertex,
			ToRenderPosition(source, isBlender)
		);
	}

	if(!hasVertex) return false;
	outBounds = bounds;
	return true;
}

inline bool HasSkinnedMesh(const aiScene& scene) noexcept {
	for(unsigned int meshIndex = 0; meshIndex < scene.mNumMeshes; ++meshIndex){
		const aiMesh* mesh = scene.mMeshes[meshIndex];
		if(mesh && mesh->HasBones()) return true;
	}
	return false;
}

inline bool TryBuildLocalBounds(
	const aiScene& scene,
	bool isBlender,
	EntityAABB& outBounds
) noexcept {
	bool hasVertex = false;
	EntityAABB bounds{};

	for(unsigned int meshIndex = 0; meshIndex < scene.mNumMeshes; ++meshIndex){
		const aiMesh* mesh = scene.mMeshes[meshIndex];
		if(!mesh || !mesh->HasPositions()) continue;

		for(unsigned int vertexIndex = 0;
			vertexIndex < mesh->mNumVertices;
			++vertexIndex){
			ExpandBounds(
				bounds,
				hasVertex,
				ToRenderPosition(mesh->mVertices[vertexIndex], isBlender)
			);
		}
	}

	if(!hasVertex) return false;
	outBounds = bounds;
	return true;
}

inline std::uint64_t MakeSourceRevision(
	const ModelRendererComponent& renderer
) noexcept {
	const std::uint64_t modelAddress = static_cast<std::uint64_t>(
		reinterpret_cast<std::uintptr_t>(renderer.model.get())
	);
	const std::uint64_t sceneAddress = static_cast<std::uint64_t>(
		reinterpret_cast<std::uintptr_t>(
			renderer.model ? renderer.model->AiScene : nullptr
		)
	);
	const std::uint64_t pathHash = static_cast<std::uint64_t>(
		std::hash<std::string_view>{}(renderer.modelFilePath)
	);

	std::uint64_t revision = modelAddress;
	revision ^= sceneAddress + 0x9e3779b97f4a7c15ull +
		(revision << 6) + (revision >> 2);
	revision ^= pathHash + 0x9e3779b97f4a7c15ull +
		(revision << 6) + (revision >> 2);
	revision ^= renderer.isBlender ? 0x6a09e667f3bcc909ull : 0ull;
	return revision == 0 ? 1 : revision;
}

struct UpdateResult {
	size_t visited = 0;
	size_t updated = 0;
	size_t unavailable = 0;
	size_t skippedSkinned = 0;
};

// Static ModelだけLocal Boundsを供給する。
// Skinned ModelはAnimation中の形状をRest Pose Boundsで切らないよう未確定のまま残す。
inline UpdateResult UpdateScene(ComponentRegistry& components){
	UpdateResult result;
	const auto entities =
		components.FindEntitiesWithComponent<CullingComponent>();

	for(Entity entity : entities){
		ModelRendererComponent* renderer =
			components.GetComponent<ModelRendererComponent>(entity);
		if(!renderer) continue;

		++result.visited;
		CullingComponent* culling =
			components.GetComponent<CullingComponent>(entity);
		if(!culling || !renderer->model || !renderer->model->AiScene){
			if(culling){
				culling->sourceRevision = 0;
				culling->boundsValid = false;
			}
			++result.unavailable;
			continue;
		}

		const aiScene& scene = *renderer->model->AiScene;
		if(HasSkinnedMesh(scene)){
			culling->sourceRevision = 0;
			culling->boundsValid = false;
			++result.skippedSkinned;
			continue;
		}

		const std::uint64_t revision = MakeSourceRevision(*renderer);
		if(culling->sourceRevision == revision) continue;

		EntityAABB localBounds;
		if(!TryBuildLocalBounds(scene, renderer->isBlender, localBounds)){
			culling->sourceRevision = 0;
			culling->boundsValid = false;
			++result.unavailable;
			continue;
		}

		culling->localBounds = localBounds;
		culling->sourceRevision = revision;
		culling->boundsValid = false;
		++result.updated;
	}
	return result;
}

} // namespace ModelCullingBoundsProvider
