#include <cassert>
#include <cmath>
#include <string>

#include "Engine/Scene/System/Physic/PhysicsSimulationOverlapAnalysis.h"

namespace {

SystemTaskProfileSample MakeSample(
	const char* name,
	double begin,
	double end,
	std::size_t worker,
	bool succeeded = true
){
	SystemTaskProfileSample sample;
	sample.taskName = name;
	sample.workerIndex = worker;
	sample.startMilliseconds = begin;
	sample.endMilliseconds = end;
	sample.durationMilliseconds = end - begin;
	sample.succeeded = succeeded;
	return sample;
}

bool NearlyEqual(double lhs, double rhs){
	return std::fabs(lhs - rhs) < 1.0e-9;
}

SystemScheduleProfileSnapshot MakeFixedSnapshot(){
	SystemScheduleProfileSnapshot snapshot;
	snapshot.domain = SystemTaskDomain::Fixed;
	snapshot.jobSystemRunning = true;
	snapshot.workerCount = 4;
	return snapshot;
}

void ValidateMissingSamples(){
	SystemScheduleProfileSnapshot snapshot = MakeFixedSnapshot();
	const auto result = PhysicsSimulationOverlapAnalysis::Analyze(snapshot);
	assert(!result.available);

	snapshot.domain = SystemTaskDomain::Frame;
	snapshot.samples.push_back(MakeSample(
		"PhysicSystem.Simulation.Simulate", 0.0, 0.1, 0
	));
	snapshot.samples.push_back(MakeSample(
		"PhysicSystem.Simulation.Fetch", 0.1, 1.0, 1
	));
	assert(!PhysicsSimulationOverlapAnalysis::Analyze(snapshot).available);
}

void ValidateNegligibleFetch(){
	SystemScheduleProfileSnapshot snapshot = MakeFixedSnapshot();
	snapshot.samples.push_back(MakeSample(
		"PhysicSystem.Simulation.Simulate", 0.0, 0.05, 0
	));
	snapshot.samples.push_back(MakeSample(
		"PhysicSystem.Simulation.Fetch", 0.08, 0.13, 1
	));

	const auto result = PhysicsSimulationOverlapAnalysis::Analyze(snapshot);
	assert(result.available);
	assert(result.fetchNegligible);
	assert(result.overlapEffective);
	assert(!result.deeperPipeliningCandidate);
	assert(NearlyEqual(result.submissionGapMilliseconds, 0.03));
}

void ValidateEffectiveOverlapAndUnion(){
	SystemScheduleProfileSnapshot snapshot = MakeFixedSnapshot();
	snapshot.samples.push_back(MakeSample(
		"PhysicSystem.Simulation.Simulate", 0.0, 0.1, 0
	));
	snapshot.samples.push_back(MakeSample(
		"PhysicSystem.Simulation.Fetch", 0.2, 2.2, 1
	));
	// 0.2-1.4 と 1.0-2.2 は重なるため、重複加算せず2.0msを覆う。
	snapshot.samples.push_back(MakeSample(
		"Gameplay.AI.Update", 0.2, 1.4, 2
	));
	snapshot.samples.push_back(MakeSample(
		"Animation.Pose.Build", 1.0, 2.2, 3
	));

	const auto result = PhysicsSimulationOverlapAnalysis::Analyze(snapshot);
	assert(result.available);
	assert(result.overlappingTaskCount == 2);
	assert(NearlyEqual(result.fetchMilliseconds, 2.0));
	assert(NearlyEqual(result.overlappedFetchMilliseconds, 2.0));
	assert(NearlyEqual(result.uncoveredFetchMilliseconds, 0.0));
	assert(NearlyEqual(result.coverageRatio, 1.0));
	assert(result.overlapEffective);
	assert(!result.deeperPipeliningCandidate);
}

void ValidatePartialOverlap(){
	SystemScheduleProfileSnapshot snapshot = MakeFixedSnapshot();
	snapshot.samples.push_back(MakeSample(
		"PhysicSystem.Simulation.Simulate", 0.0, 0.1, 0
	));
	snapshot.samples.push_back(MakeSample(
		"PhysicSystem.Simulation.Fetch", 0.1, 1.1, 1
	));
	snapshot.samples.push_back(MakeSample(
		"Gameplay.Fixed.Work", 0.3, 0.8, 2
	));

	const auto result = PhysicsSimulationOverlapAnalysis::Analyze(snapshot);
	assert(result.available);
	assert(NearlyEqual(result.overlappedFetchMilliseconds, 0.5));
	assert(NearlyEqual(result.uncoveredFetchMilliseconds, 0.5));
	assert(NearlyEqual(result.coverageRatio, 0.5));
	assert(!result.overlapEffective);
	assert(result.deeperPipeliningCandidate);
}

void ValidateFailedAndDominatedFetch(){
	SystemScheduleProfileSnapshot failed = MakeFixedSnapshot();
	failed.samples.push_back(MakeSample(
		"PhysicSystem.Simulation.Simulate", 0.0, 0.1, 0
	));
	failed.samples.push_back(MakeSample(
		"PhysicSystem.Simulation.Fetch", 0.1, 1.1, 1, false
	));
	assert(!PhysicsSimulationOverlapAnalysis::Analyze(failed).available);

	SystemScheduleProfileSnapshot dominated = MakeFixedSnapshot();
	dominated.samples.push_back(MakeSample(
		"PhysicSystem.Simulation.Simulate", 0.0, 0.1, 0
	));
	dominated.samples.push_back(MakeSample(
		"PhysicSystem.Simulation.Fetch", 0.1, 1.1, 1
	));
	const auto result = PhysicsSimulationOverlapAnalysis::Analyze(dominated);
	assert(result.available);
	assert(NearlyEqual(result.uncoveredFetchMilliseconds, 1.0));
	assert(result.deeperPipeliningCandidate);
}

} // namespace

int main(){
	ValidateMissingSamples();
	ValidateNegligibleFetch();
	ValidateEffectiveOverlapAndUnion();
	ValidatePartialOverlap();
	ValidateFailedAndDominatedFetch();
	return 0;
}
