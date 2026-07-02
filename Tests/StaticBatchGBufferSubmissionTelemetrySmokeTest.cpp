#include <cassert>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchGBufferSubmissionTelemetry.h"

int main(){
	StaticBatchGBufferSubmissionTelemetry telemetry;
	telemetry.sourceRevision = 44;
	telemetry.submittedGroupCount = 3;
	telemetry.submittedInstanceCount = 10;
	telemetry.visibleInstanceCount = 12;
	telemetry.culledInstanceCount = 8;
	telemetry.compactedMixedGroupCount = 2;
	telemetry.visibilitySelectionFailureCount = 1;
	telemetry.pipelineReady = true;
	telemetry.instanceUploadReady = true;

	assert(telemetry.StaticDrawCallCount() == 3);
	assert(telemetry.ReplacedOrdinaryDrawCallCount() == 10);
	assert(telemetry.EstimatedDrawCallReduction() == 7);

	telemetry.submittedGroupCount = 5;
	telemetry.submittedInstanceCount = 2;
	assert(telemetry.EstimatedDrawCallReduction() == 0);

	telemetry.Reset(99);
	assert(telemetry.sourceRevision == 99);
	assert(telemetry.submittedGroupCount == 0);
	assert(telemetry.submittedInstanceCount == 0);
	assert(telemetry.visibleInstanceCount == 0);
	assert(telemetry.culledInstanceCount == 0);
	assert(telemetry.compactedMixedGroupCount == 0);
	assert(telemetry.visibilitySelectionFailureCount == 0);
	assert(telemetry.EstimatedDrawCallReduction() == 0);
	assert(!telemetry.pipelineReady);
	assert(!telemetry.instanceUploadReady);
	return 0;
}
