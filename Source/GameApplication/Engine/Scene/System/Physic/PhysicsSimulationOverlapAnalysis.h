// =======================================================================
//
// PhysicsSimulationOverlapAnalysis.h
//
// Step 17-F: Schedule ProfilerのFixed Captureから、PhysX Fetch待機中に
// 他Taskが実行された時間を算出する。Cross-frame化は計測結果なしに行わない。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

#include "System/Scheduler/SystemScheduleProfiler.h"

namespace PhysicsSimulationOverlapAnalysis {

inline constexpr std::string_view SimulateTaskName =
	"PhysicSystem.Simulation.Simulate";
inline constexpr std::string_view FetchTaskName =
	"PhysicSystem.Simulation.Fetch";

inline constexpr double NegligibleFetchMilliseconds = 0.10;
inline constexpr double MeaningfulFetchMilliseconds = 0.50;
inline constexpr double MeaningfulUncoveredMilliseconds = 0.25;
inline constexpr double EffectiveCoverageRatio = 0.75;

struct Result {
	bool available = false;
	double simulateMilliseconds = 0.0;
	double submissionGapMilliseconds = 0.0;
	double fetchMilliseconds = 0.0;
	double overlappedFetchMilliseconds = 0.0;
	double uncoveredFetchMilliseconds = 0.0;
	double coverageRatio = 0.0;
	std::size_t overlappingTaskCount = 0;
	bool fetchNegligible = false;
	bool overlapEffective = false;
	bool deeperPipeliningCandidate = false;
};

namespace Detail {

struct Interval {
	double begin = 0.0;
	double end = 0.0;
};

inline const SystemTaskProfileSample* FindTask(
	const SystemScheduleProfileSnapshot& snapshot,
	std::string_view taskName
) noexcept {
	for(const SystemTaskProfileSample& sample : snapshot.samples){
		if(sample.taskName == taskName){
			return &sample;
		}
	}
	return nullptr;
}

inline double MeasureUnion(std::vector<Interval>& intervals) noexcept {
	if(intervals.empty()) return 0.0;

	std::sort(
		intervals.begin(),
		intervals.end(),
		[](const Interval& lhs, const Interval& rhs){
			if(lhs.begin != rhs.begin) return lhs.begin < rhs.begin;
			return lhs.end < rhs.end;
		}
	);

	double covered = 0.0;
	double currentBegin = intervals.front().begin;
	double currentEnd = intervals.front().end;
	for(std::size_t index = 1; index < intervals.size(); ++index){
		const Interval& interval = intervals[index];
		if(interval.begin <= currentEnd){
			currentEnd = (std::max)(currentEnd, interval.end);
			continue;
		}
		covered += (std::max)(0.0, currentEnd - currentBegin);
		currentBegin = interval.begin;
		currentEnd = interval.end;
	}
	covered += (std::max)(0.0, currentEnd - currentBegin);
	return covered;
}

} // namespace Detail

inline Result Analyze(const SystemScheduleProfileSnapshot& snapshot){
	Result result;
	if(snapshot.domain != SystemTaskDomain::Fixed){
		return result;
	}

	const SystemTaskProfileSample* simulate =
		Detail::FindTask(snapshot, SimulateTaskName);
	const SystemTaskProfileSample* fetch =
		Detail::FindTask(snapshot, FetchTaskName);
	if(!simulate || !fetch || !simulate->succeeded || !fetch->succeeded){
		return result;
	}

	const double fetchBegin = fetch->startMilliseconds;
	const double fetchEnd = (std::max)(fetchBegin, fetch->endMilliseconds);
	result.available = true;
	result.simulateMilliseconds = (std::max)(0.0, simulate->durationMilliseconds);
	result.submissionGapMilliseconds = (std::max)(
		0.0,
		fetchBegin - simulate->endMilliseconds
	);
	result.fetchMilliseconds = (std::max)(0.0, fetchEnd - fetchBegin);

	std::vector<Detail::Interval> overlapIntervals;
	overlapIntervals.reserve(snapshot.samples.size());
	for(const SystemTaskProfileSample& sample : snapshot.samples){
		if(&sample == fetch || !sample.succeeded){
			continue;
		}

		const double overlapBegin = (std::max)(
			fetchBegin,
			sample.startMilliseconds
		);
		const double overlapEnd = (std::min)(
			fetchEnd,
			sample.endMilliseconds
		);
		if(overlapEnd <= overlapBegin){
			continue;
		}

		overlapIntervals.push_back({overlapBegin, overlapEnd});
		++result.overlappingTaskCount;
	}

	result.overlappedFetchMilliseconds = (std::min)(
		result.fetchMilliseconds,
		Detail::MeasureUnion(overlapIntervals)
	);
	result.uncoveredFetchMilliseconds = (std::max)(
		0.0,
		result.fetchMilliseconds - result.overlappedFetchMilliseconds
	);
	if(result.fetchMilliseconds > 0.0){
		result.coverageRatio = result.overlappedFetchMilliseconds /
			result.fetchMilliseconds;
	}

	result.fetchNegligible =
		result.fetchMilliseconds <= NegligibleFetchMilliseconds;
	result.overlapEffective = result.fetchNegligible ||
		result.coverageRatio >= EffectiveCoverageRatio ||
		result.uncoveredFetchMilliseconds <= NegligibleFetchMilliseconds;
	result.deeperPipeliningCandidate =
		result.fetchMilliseconds >= MeaningfulFetchMilliseconds &&
		result.uncoveredFetchMilliseconds >= MeaningfulUncoveredMilliseconds &&
		!result.overlapEffective;
	return result;
}

} // namespace PhysicsSimulationOverlapAnalysis
