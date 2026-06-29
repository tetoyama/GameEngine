#include <cassert>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchShadowSubmission.h"

int main(){
	StaticBatchShadowSubmissionTelemetry telemetry;
	telemetry.submittedGroupCount = 2;
	telemetry.submittedInstanceCount = 7;
	assert(telemetry.EstimatedDrawCallReduction() == 5);

	telemetry.submittedGroupCount = 7;
	telemetry.submittedInstanceCount = 2;
	assert(telemetry.EstimatedDrawCallReduction() == 0);
	return 0;
}
