#pragma once

#include <cstddef>

struct StaticBatchShadowSubmissionTelemetry {
	std::size_t lightTileCount = 0;
	std::size_t submissionAttemptCount = 0;
	std::size_t candidateGroupCount = 0;
	std::size_t submittedGroupCount = 0;
	std::size_t submittedInstanceCount = 0;
	std::size_t mappingFallbackCount = 0;
	std::size_t eligibilityFallbackCount = 0;
	std::size_t layerFallbackCount = 0;
	std::size_t geometryFallbackCount = 0;
	std::size_t drawFailureCount = 0;
	std::size_t queueFailureCount = 0;

	void Reset() noexcept {
		*this = {};
	}

	void AccumulateSubmission(
		const StaticBatchShadowSubmissionTelemetry& other
	) noexcept {
		candidateGroupCount += other.candidateGroupCount;
		submittedGroupCount += other.submittedGroupCount;
		submittedInstanceCount += other.submittedInstanceCount;
		mappingFallbackCount += other.mappingFallbackCount;
		eligibilityFallbackCount += other.eligibilityFallbackCount;
		layerFallbackCount += other.layerFallbackCount;
		geometryFallbackCount += other.geometryFallbackCount;
		drawFailureCount += other.drawFailureCount;
		queueFailureCount += other.queueFailureCount;
	}

	std::size_t ReplacedOrdinaryDrawCount() const noexcept {
		return submittedInstanceCount;
	}

	std::size_t StaticDrawCallCount() const noexcept {
		return submittedGroupCount;
	}

	std::size_t EstimatedDrawCallReduction() const noexcept {
		return submittedInstanceCount > submittedGroupCount
			? submittedInstanceCount - submittedGroupCount
			: 0;
	}
};
