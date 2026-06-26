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
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Component/terrainComponent.h"
#include "Scene/Component/waveComponent.h"
#include "Scene/Registry/componentRegistry.h"

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
	std::uint64_t revision = static_cast<std::uint64_t>(
		static_cast<std::uint32_t>(wave.Resolution)
	);
	const std::uint32_t amplitudeBits = std::bit_cast<std::uint32_t>(
		wave.Amplitude
	);
	revision ^= static_cast<std::uint64_t>(amplitudeBits) +
		0x9e3779b97f4a7c15ull + (revision << 6) + (revision >> 2);
	return revision == 0 ? 1 : revision;
}

struct UpdateResult {
	size_t visited = 0;
	size_t updated = 0;
	size_t unavailable = 0;
	size_t skippedHigherPrioritySource = 0;
};

// Bounds source priority: Model > Terrain > Wave.
inline UpdateResult UpdateScene(ComponentRegistry& components){
	UpdateResult result;
	const auto entities =
		components.FindEntitiesWithComponent<CullingComponent>();

	for(Entity entity : entities){
		WaveComponent* wave = components.GetComponent<WaveComponent>(entity);
		if(!wave) continue;

		++result.visited;
		if(components.GetComponent<ModelRendererComponent>(entity) ||
			components.GetComponent<TerrainComponent>(entity)){
			++result.skippedHigherPrioritySource;
			continue;
		}

		CullingComponent* culling =
			components.GetComponent<CullingComponent>(entity);
		if(!culling){
			++result.unavailable;
			continue;
		}

		const std::uint64_t revision = MakeSourceRevision(*wave);
		if(culling->sourceRevision == revision) continue;

		EntityAABB localBounds;
		if(!TryBuildLocalBounds(*wave, localBounds)){
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

} // namespace WaveCullingBoundsProvider
