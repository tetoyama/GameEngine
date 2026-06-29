#pragma once

#include <cstddef>

struct StaticBatchShadowSubmissionTelemetry {
	std::size_t lightTileCount = 0;
	std::size_t submissionAttemptCount = 0;
	std::size_t candidateGroupCount = 0;
	std::size_t visibleInstanceCount = 0;
	std::size_t culledInstanceCount = 0;
	std::size_t fullyCulledGroupCount = 0;
	std::size_t compactedMixedGroupCount = 0;
	std::size_t singleInstanceFallbackCount = 0;
	std::size_t visibilityFailureCount = 0;
	std::size_t instanceUploadFailureCount = 0;
	std::size_t submittedGroupCount = 0;
	std::size_t submittedInstanceCount = 0;
	std::size_t mappingFallbackCount = 0;
	std::size_t eligibilityFallbackCount = 0;
	std::size_t textureBindingFallbackCount = 0;
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
		visibleInstanceCount += other.visibleInstanceCount;
		culledInstanceCount += other.culledInstanceCount;
		fullyCulledGroupCount += other.fullyCulledGroupCount;
		compactedMixedGroupCount += other.compactedMixedGroupCount;
		singleInstanceFallbackCount += other.singleInstanceFallbackCount;
		visibilityFailureCount += other.visibilityFailureCount;
		instanceUploadFailureCount += other.instanceUploadFailureCount;
		submittedGroupCount += other.submittedGroupCount;
		submittedInstanceCount += other.submittedInstanceCount;
		mappingFallbackCount += other.mappingFallbackCount;
		eligibilityFallbackCount += other.eligibilityFallbackCount;
		textureBindingFallbackCount += other.textureBindingFallbackCount;
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
