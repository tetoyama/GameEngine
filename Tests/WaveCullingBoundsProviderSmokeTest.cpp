#include <cassert>
#include <limits>

#include "Engine/Scene/System/Render/Culling/WaveCullingBoundsProvider.h"

int main(){
	WaveComponent wave;
	wave.Resolution = 16;
	wave.Amplitude = 2.5f;

	EntityAABB bounds;
	assert(WaveCullingBoundsProvider::TryBuildLocalBounds(wave, bounds));
	assert(bounds.min.x == -1.0f);
	assert(bounds.min.y == -2.5f);
	assert(bounds.min.z == -1.0f);
	assert(bounds.max.x == 1.0f);
	assert(bounds.max.y == 2.5f);
	assert(bounds.max.z == 1.0f);

	wave.Amplitude = -3.0f;
	assert(WaveCullingBoundsProvider::TryBuildLocalBounds(wave, bounds));
	assert(bounds.min.y == -3.0f);
	assert(bounds.max.y == 3.0f);

	wave.Amplitude = 1.0f;
	wave.Time = 0.0f;
	const auto firstRevision = WaveCullingBoundsProvider::MakeSourceRevision(wave);
	wave.Time = 100.0f;
	const auto timeRevision = WaveCullingBoundsProvider::MakeSourceRevision(wave);
	assert(firstRevision == timeRevision);

	wave.Amplitude = 1.5f;
	const auto amplitudeRevision = WaveCullingBoundsProvider::MakeSourceRevision(wave);
	assert(firstRevision != amplitudeRevision);

	wave.Resolution = 0;
	assert(!WaveCullingBoundsProvider::TryBuildLocalBounds(wave, bounds));
	wave.Resolution = 16;
	wave.Amplitude = (std::numeric_limits<float>::infinity)();
	assert(!WaveCullingBoundsProvider::TryBuildLocalBounds(wave, bounds));
	return 0;
}
