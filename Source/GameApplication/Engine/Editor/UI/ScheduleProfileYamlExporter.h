// =======================================================================
//
// ScheduleProfileYamlExporter.h
//
// Schedule ProfilerのCompiled Graphと実測Snapshotをyaml-cppで出力する。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "Scene/Registry/systemRegistry.h"

namespace ScheduleProfileYamlExporter {

using SnapshotSet =
	std::array<std::optional<SystemScheduleProfileSnapshot>, 4>;

struct ExportResult {
	bool succeeded = false;
	std::filesystem::path outputPath;
	std::string error;
	size_t exportedDomainCount = 0;
};

inline const char* DomainName(SystemTaskDomain domain) noexcept {
	switch(domain) {
	case SystemTaskDomain::Fixed: return "Fixed";
	case SystemTaskDomain::Frame: return "Frame";
	case SystemTaskDomain::Editor: return "Editor";
	case SystemTaskDomain::Render: return "Render";
	}
	return "Unknown";
}

inline const char* PhaseName(SystemPhase phase) noexcept {
	switch(phase) {
	case SystemPhase::Earliest: return "Earliest";
	case SystemPhase::Early: return "Early";
	case SystemPhase::Default: return "Default";
	case SystemPhase::Late: return "Late";
	case SystemPhase::Latest: return "Latest";
	}
	return "Custom";
}

inline const char* AffinityName(ThreadAffinity affinity) noexcept {
	switch(affinity) {
	case ThreadAffinity::AnyWorker: return "AnyWorker";
	case ThreadAffinity::MainThread: return "MainThread";
	case ThreadAffinity::RenderThread: return "RenderThread";
	}
	return "Unknown";
}

inline const char* StructuralAccessName(StructuralAccess access) noexcept {
	switch(access) {
	case StructuralAccess::None: return "None";
	case StructuralAccess::EmitCommands: return "EmitCommands";
	case StructuralAccess::ExclusiveWorldWrite:
		return "ExclusiveWorldWrite";
	}
	return "Unknown";
}

inline const char* WorldAccessName(WorldAccessMode access) noexcept {
	switch(access) {
	case WorldAccessMode::None: return "None";
	case WorldAccessMode::Exclusive: return "Exclusive";
	}
	return "Unknown";
}

inline SnapshotSet CollectLatestSnapshots(SystemScheduleProfiler& profiler) {
	SnapshotSet snapshots;
	for(size_t index = 0; index < snapshots.size(); ++index) {
		snapshots[index] = profiler.GetLatestSnapshot(
			static_cast<SystemTaskDomain>(index)
		);
	}
	return snapshots;
}

inline bool HasAnySnapshot(const SnapshotSet& snapshots) noexcept {
	return std::any_of(
		snapshots.begin(),
		snapshots.end(),
		[](const auto& snapshot) { return snapshot.has_value(); }
	);
}

inline size_t CountDependencyEdges(
	const CompiledSystemSchedule& schedule
) noexcept {
	size_t count = 0;
	for(const SystemScheduleNode& node : schedule.nodes) {
		count += node.dependencies.size();
	}
	return count;
}

struct CaptureSummary {
	size_t peakConcurrency = 0;
	size_t workerLaneCount = 0;
	bool mainThreadUsed = false;
};

inline CaptureSummary CalculateCaptureSummary(
	const SystemScheduleProfileSnapshot& snapshot
) {
	struct Event {
		double timestamp = 0.0;
		int delta = 0;
	};

	std::vector<Event> events;
	events.reserve(snapshot.samples.size() * 2);
	std::set<size_t> workerLanes;
	bool mainThreadUsed = false;

	for(const SystemTaskProfileSample& sample : snapshot.samples) {
		events.push_back({sample.startMilliseconds, 1});
		events.push_back({sample.endMilliseconds, -1});
		if(sample.workerIndex == JobSystem::InvalidWorkerIndex) {
			mainThreadUsed = true;
		} else {
			workerLanes.insert(sample.workerIndex);
		}
	}

	std::sort(
		events.begin(),
		events.end(),
		[](const Event& lhs, const Event& rhs) {
			if(lhs.timestamp != rhs.timestamp) {
				return lhs.timestamp < rhs.timestamp;
			}
			// 同時刻では終了を先に処理し、接しているだけの区間を
			// 重複として数えない。
			return lhs.delta < rhs.delta;
		}
	);

	int active = 0;
	size_t peak = 0;
	for(const Event& event : events) {
		active += event.delta;
		peak = (std::max)(
			peak,
			static_cast<size_t>((std::max)(active, 0))
		);
	}

	return {peak, workerLanes.size(), mainThreadUsed};
}

inline YAML::Node EncodeIndexSequence(const std::vector<size_t>& indices) {
	YAML::Node sequence(YAML::NodeType::Sequence);
	for(size_t index : indices) {
		sequence.push_back(static_cast<uint64_t>(index));
	}
	return sequence;
}

inline YAML::Node EncodeAccess(const SystemAccess& access) {
	YAML::Node node;
	node["World"] = WorldAccessName(access.worldAccess);
	node["Structural"] = StructuralAccessName(access.structuralAccess);
	node["ComponentReadCount"] =
		static_cast<uint64_t>(access.componentReads.size());
	node["ComponentWriteCount"] =
		static_cast<uint64_t>(access.componentWrites.size());
	node["ResourceReadCount"] =
		static_cast<uint64_t>(access.resourceReads.size());
	node["ResourceWriteCount"] =
		static_cast<uint64_t>(access.resourceWrites.size());
	return node;
}

inline YAML::Node EncodeCompiledSchedule(
	const CompiledSystemSchedule& schedule,
	const std::vector<SystemTask>& tasks
) {
	YAML::Node result;
	result["Valid"] = schedule.valid;
	result["Error"] = schedule.error;
	result["TaskCount"] = static_cast<uint64_t>(schedule.nodes.size());
	result["DependencyEdgeCount"] =
		static_cast<uint64_t>(CountDependencyEdges(schedule));

	YAML::Node nodes(YAML::NodeType::Sequence);
	for(size_t nodeIndex = 0; nodeIndex < schedule.nodes.size(); ++nodeIndex) {
		const SystemScheduleNode& scheduleNode = schedule.nodes[nodeIndex];
		const SystemTask& task = tasks[scheduleNode.taskIndex];

		YAML::Node node;
		node["NodeIndex"] = static_cast<uint64_t>(nodeIndex);
		node["TaskIndex"] = static_cast<uint64_t>(scheduleNode.taskIndex);
		node["Name"] = task.name;
		node["System"] = task.owner && task.owner->GetSystemName()
			? task.owner->GetSystemName()
			: "Unknown";
		node["Phase"] = PhaseName(task.order.phase);
		node["PhaseValue"] = static_cast<int32_t>(task.order.phase);
		node["Priority"] = task.order.priority;
		node["RegistrationOrder"] = task.order.registrationOrder;
		node["DeclaredAffinity"] = AffinityName(task.threadAffinity);
		node["Access"] = EncodeAccess(task.access);
		node["Dependencies"] = EncodeIndexSequence(scheduleNode.dependencies);
		node["Dependents"] = EncodeIndexSequence(scheduleNode.dependents);
		nodes.push_back(node);
	}
	result["Nodes"] = nodes;

	YAML::Node executionOrder(YAML::NodeType::Sequence);
	for(size_t taskIndex : schedule.executionOrder) {
		executionOrder.push_back(static_cast<uint64_t>(taskIndex));
	}
	result["ExecutionOrderTaskIndices"] = executionOrder;
	return result;
}

inline YAML::Node EncodeCapture(
	const SystemScheduleProfileSnapshot& snapshot
) {
	const CaptureSummary summary = CalculateCaptureSummary(snapshot);

	YAML::Node result;
	result["Available"] = true;
	result["CaptureIndex"] = snapshot.captureIndex;
	result["JobSystemRunning"] = snapshot.jobSystemRunning;
	result["WorkerCount"] = static_cast<uint64_t>(snapshot.workerCount);
	result["TotalMilliseconds"] = snapshot.totalMilliseconds;
	result["PeakConcurrency"] = static_cast<uint64_t>(summary.peakConcurrency);
	result["WorkerLaneCount"] = static_cast<uint64_t>(summary.workerLaneCount);
	result["MainThreadUsed"] = summary.mainThreadUsed;

	YAML::Node samples(YAML::NodeType::Sequence);
	for(const SystemTaskProfileSample& sample : snapshot.samples) {
		YAML::Node node;
		node["NodeIndex"] = static_cast<uint64_t>(sample.nodeIndex);
		node["TaskIndex"] = static_cast<uint64_t>(sample.taskIndex);
		node["Name"] = sample.taskName;
		node["Phase"] = PhaseName(sample.phase);
		node["PhaseValue"] = static_cast<int32_t>(sample.phase);
		node["Priority"] = sample.priority;
		node["RegistrationOrder"] = sample.registrationOrder;
		node["DeclaredAffinity"] = AffinityName(sample.declaredAffinity);

		YAML::Node actualThread;
		if(sample.workerIndex == JobSystem::InvalidWorkerIndex) {
			actualThread["Kind"] = "MainThread";
			actualThread["WorkerIndex"] = YAML::Null;
		} else {
			actualThread["Kind"] = "Worker";
			actualThread["WorkerIndex"] =
				static_cast<uint64_t>(sample.workerIndex);
		}
		node["ActualThread"] = actualThread;

		node["StartMilliseconds"] = sample.startMilliseconds;
		node["EndMilliseconds"] = sample.endMilliseconds;
		node["DurationMilliseconds"] = sample.durationMilliseconds;
		node["Succeeded"] = sample.succeeded;
		samples.push_back(node);
	}
	result["Tasks"] = samples;
	return result;
}

inline std::string BuildTimestamp(
	const std::chrono::system_clock::time_point& timePoint,
	const char* format
) {
	const std::time_t rawTime =
		std::chrono::system_clock::to_time_t(timePoint);
	std::tm localTime{};
	localtime_s(&localTime, &rawTime);

	std::ostringstream stream;
	stream << std::put_time(&localTime, format);
	return stream.str();
}

inline std::string BuildExportedAt(
	const std::chrono::system_clock::time_point& timePoint
) {
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
		timePoint.time_since_epoch()
	).count() % 1000;

	std::ostringstream stream;
	stream << BuildTimestamp(timePoint, "%Y-%m-%dT%H:%M:%S")
		<< '.' << std::setw(3) << std::setfill('0') << milliseconds;
	return stream.str();
}

inline std::string BuildFilename(
	const std::chrono::system_clock::time_point& timePoint
) {
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
		timePoint.time_since_epoch()
	).count() % 1000;

	std::ostringstream stream;
	stream << "ScheduleCapture_"
		<< BuildTimestamp(timePoint, "%Y%m%d_%H%M%S")
		<< '_' << std::setw(3) << std::setfill('0') << milliseconds
		<< ".yaml";
	return stream.str();
}

inline ExportResult Export(
	SystemRegistry& registry,
	const SnapshotSet& snapshots,
	bool frozenCapture,
	const std::filesystem::path& outputDirectory =
		std::filesystem::path("Logs") / "Schedule"
) {
	ExportResult result;
	if(!HasAnySnapshot(snapshots)) {
		result.error = "No completed schedule captures are available.";
		return result;
	}

	std::error_code fileSystemError;
	std::filesystem::create_directories(outputDirectory, fileSystemError);
	if(fileSystemError) {
		result.error = "Failed to create schedule log directory: " +
			fileSystemError.message();
		return result;
	}

	const auto exportTime = std::chrono::system_clock::now();
	result.outputPath = outputDirectory / BuildFilename(exportTime);

	YAML::Node root;
	root["Schema"] = "GameEngine.SystemScheduleCapture";
	root["Version"] = 1;
	root["ExportedAtLocal"] = BuildExportedAt(exportTime);
	root["CaptureSource"] = frozenCapture ? "Frozen" : "Latest";

	YAML::Node jobSystem;
	jobSystem["Running"] = registry.GetJobSystem().IsRunning();
	jobSystem["WorkerCount"] =
		static_cast<uint64_t>(registry.GetJobSystem().WorkerCount());
	root["JobSystem"] = jobSystem;

	const std::vector<SystemTask>& tasks = registry.GetTasks();
	YAML::Node domains(YAML::NodeType::Sequence);
	for(size_t domainIndex = 0; domainIndex < snapshots.size(); ++domainIndex) {
		const SystemTaskDomain domain =
			static_cast<SystemTaskDomain>(domainIndex);
		const CompiledSystemSchedule& schedule = registry.GetSchedule(domain);

		YAML::Node domainNode;
		domainNode["Name"] = DomainName(domain);
		domainNode["Value"] = static_cast<uint32_t>(domain);
		domainNode["CompiledSchedule"] =
			EncodeCompiledSchedule(schedule, tasks);

		if(snapshots[domainIndex]) {
			domainNode["Capture"] = EncodeCapture(*snapshots[domainIndex]);
			++result.exportedDomainCount;
		} else {
			YAML::Node capture;
			capture["Available"] = false;
			domainNode["Capture"] = capture;
		}

		domains.push_back(domainNode);
	}
	root["Domains"] = domains;

	YAML::Emitter emitter;
	emitter.SetIndent(2);
	emitter << root;
	if(!emitter.good()) {
		result.error = "yaml-cpp failed to emit the schedule capture.";
		return result;
	}

	std::ofstream output(result.outputPath, std::ios::out | std::ios::trunc);
	if(!output) {
		result.error = "Failed to open the schedule log file for writing.";
		return result;
	}

	output << emitter.c_str();
	output.flush();
	if(!output) {
		result.error = "Failed while writing the schedule log file.";
		return result;
	}

	result.succeeded = true;
	return result;
}

} // namespace ScheduleProfileYamlExporter
