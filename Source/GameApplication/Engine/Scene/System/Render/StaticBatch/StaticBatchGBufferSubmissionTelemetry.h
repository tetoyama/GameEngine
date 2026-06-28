#pragma once

#include <cstddef>
#include <cstdint>

struct StaticBatchGBufferSubmissionTelemetry {
	std::uint64_t sourceRevision = 0;
	std::size_t candidateGroupCount = 0;
	std::size_t submittedGroupCount = 0;
	std::size_t submittedInstanceCount = 0;
	std::size_t singleInstanceFallbackCount = 0;
	std::size_t packetRangeFallbackCount = 0;
	std::size_t layerFallbackCount = 0;
	std::size_t culledGroupCount = 0;
	std::size_t mixedVisibilityFallbackCount = 0;
	std::size_t geometryFallbackCount = 0;
	std::size_t materialFallbackCount = 0;
	std::size_t drawFailureCount = 0;
	std::size_t queueFailureCount = 0;
	std::size_t targetBindingFailureCount = 0;
	bool pipelineReady = false;
	bool instanceUploadReady = false;

	void Reset(std::uint64_t revision = 0) noexcept {
		*this = {};
		sourceRevision = revision;
	}
};
