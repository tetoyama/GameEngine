// =======================================================================
//
// WaveCullingBoundsProvider.h
//
// =======================================================================
#pragma once

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "Scene/Component/CullingComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Component/terrainComponent.h"
#include "Scene/Component/waveComponent.h"
#include "Scene/Registry/componentRegistry.h"
#include "MeshCullingBoundsProvider.h"
#include "ModelCullingBoundsProvider.h"
#include "TerrainCullingBoundsProvider.h"

namespace WaveCullingBoundsProvider {

inline bool TryBuildLocalBounds(
	const WaveComponent& wave,
	EntityAABB& outBounds
) noexcept {
	if(wave.Resolution <= 0 || !std::isfinite(wave.Amplitude)){
		return false;
	}
	const float amplitude = std::abs(wave.Amplitude);
	outBounds.min = Vector3(-1.0f, -amplitude, -1.0f);
	outBounds.max = Vector3(1.0f, amplitude, 1.0f);
	return true;
}

inline std::uint64_t MakeSourceRevision(
	const WaveComponent& wave
) noexcept {
	std::uint64_t revision = 0x57415645424f554eull;
	const auto combine = [&revision](std::uint64_t value){
		revision ^= value + 0x9e3779b97f4a7c15ull +
			(revision << 6) + (revision >> 2);
	};
	combine(static_cast<std::uint64_t>(
		static_cast<std::uint32_t>(wave.Resolution)
	));
	combine(static_cast<std::uint64_t>(
		std::bit_cast<std::uint32_t>(wave.Amplitude)
	));
	return revision == 0 ? 1 : revision;
}

struct UpdateResult {
	size_t visited = 0;
	size_t updated = 0;
	size_t unavailable = 0;
	size_t skippedHigherPrioritySource = 0;
};

inline UpdateResult UpdateScene(ComponentRegistry& components){
	UpdateResult result;
	const auto entities = components.FindEntitiesWithComponent<CullingComponent>();
	for(Entity entity : entities){
		WaveComponent* wave = components.GetComponent<WaveComponent>(entity);
		if(!wave) continue;
		++result.visited;

		if(ModelRendererComponent* model =
			components.GetComponent<ModelRendererComponent>(entity)){
			if(ModelCullingBoundsProvider::CanSupplyLocalBounds(*model)){
				++result.skippedHigherPrioritySource;
				continue;
			}
		}

		if(MeshRendererComponent* mesh =
			components.GetComponent<MeshRendererComponent>(entity)){
			EntityAABB meshBounds;
			if(MeshCullingBoundsProvider::TryBuildLocalBounds(*mesh, meshBounds)){
				++result.skippedHigherPrioritySource;
				continue;
			}
		}

		if(TerrainComponent* terrain =
			components.GetComponent<TerrainComponent>(entity)){
			if(TerrainCullingBoundsProvider::CanSupplyLocalBounds(*terrain)){
				++result.skippedHigherPrioritySource;
				continue;
			}
		}

		CullingComponent* culling =
			components.GetComponent<CullingComponent>(entity);
		if(!culling){
			++result.unavailable;
			continue;
		}

		EntityAABB localBounds;
		if(!TryBuildLocalBounds(*wave, localBounds)){
			culling->sourceRevision = 0;
			culling->boundsValid = false;
			++result.unavailable;
			continue;
		}

		const std::uint64_t revision = MakeSourceRevision(*wave);
		if(culling->sourceRevision == revision) continue;
		culling->localBounds = localBounds;
		culling->sourceRevision = revision;
		culling->boundsValid = false;
		++result.updated;
	}
	return result;
}

} // namespace WaveCullingBoundsProvider
