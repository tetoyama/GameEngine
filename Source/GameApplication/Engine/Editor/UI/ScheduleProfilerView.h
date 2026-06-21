// =======================================================================
//
// ScheduleProfilerView.h
//
// Project SettingsのScheduleタブ用Gantt表示。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Backends/ImGui/imgui.h"
#include "Scene/Registry/systemRegistry.h"

struct ScheduleProfilerViewState {
	bool freeze = false;
	float pixelsPerMillisecond = 180.0f;
	std::array<std::optional<SystemScheduleProfileSnapshot>, 4>
		frozenSnapshots;
};

namespace ScheduleProfilerView {

inline const char* DomainLabel(SystemTaskDomain domain){
	switch(domain){
	case SystemTaskDomain::Fixed: return "Fixed";
	case SystemTaskDomain::Frame: return "Frame";
	case SystemTaskDomain::Editor: return "Editor";
	case SystemTaskDomain::Render: return "Render";
	}
	return "Unknown";
}

inline const char* AffinityLabel(ThreadAffinity affinity){
	switch(affinity){
	case ThreadAffinity::AnyWorker: return "Any Worker";
	case ThreadAffinity::MainThread: return "Main Thread";
	case ThreadAffinity::RenderThread: return "Render Thread";
	}
	return "Unknown";
}

inline const char* PhaseLabel(SystemPhase phase){
	switch(phase){
	case SystemPhase::Earliest: return "Earliest";
	case SystemPhase::Early: return "Early";
	case SystemPhase::Default: return "Default";
	case SystemPhase::Late: return "Late";
	case SystemPhase::Latest: return "Latest";
	}
	return "Custom";
}

inline std::string LaneLabel(size_t workerIndex){
	if(workerIndex == JobSystem::InvalidWorkerIndex){
		return "Main";
	}
	return "Worker " + std::to_string(workerIndex);
}

inline size_t CountDependencyEdges(const CompiledSystemSchedule& schedule){
	size_t edgeCount = 0;
	for(const SystemScheduleNode& node : schedule.nodes){
		edgeCount += node.dependencies.size();
	}
	return edgeCount;
}

inline size_t CalculateDependencyLayerWidth(
	const CompiledSystemSchedule& schedule,
	const std::vector<SystemTask>& tasks,
	bool workerOnly
){
	if(schedule.nodes.empty()){
		return 0;
	}

	std::vector<size_t> levels(schedule.nodes.size(), 0);
	size_t maxLevel = 0;
	for(size_t nodeIndex = 0; nodeIndex < schedule.nodes.size(); ++nodeIndex){
		for(size_t dependency : schedule.nodes[nodeIndex].dependencies){
			levels[nodeIndex] = (std::max)(
				levels[nodeIndex],
				levels[dependency] + 1
			);
		}
		maxLevel = (std::max)(maxLevel, levels[nodeIndex]);
	}

	std::vector<size_t> counts(maxLevel + 1, 0);
	for(size_t nodeIndex = 0; nodeIndex < schedule.nodes.size(); ++nodeIndex){
		const SystemTask& task = tasks[schedule.nodes[nodeIndex].taskIndex];
		if(workerOnly && task.threadAffinity != ThreadAffinity::AnyWorker){
			continue;
		}
		++counts[levels[nodeIndex]];
	}

	return *std::max_element(counts.begin(), counts.end());
}

struct ActualConcurrencyInfo {
	size_t peakConcurrency = 0;
	size_t workerLaneCount = 0;
	bool mainLaneUsed = false;
};

inline ActualConcurrencyInfo CalculateActualConcurrency(
	const SystemScheduleProfileSnapshot& snapshot
){
	struct Event {
		double time = 0.0;
		int delta = 0;
	};

	std::vector<Event> events;
	events.reserve(snapshot.samples.size() * 2);
	std::set<size_t> workers;
	bool mainLaneUsed = false;

	for(const SystemTaskProfileSample& sample : snapshot.samples){
		events.push_back({sample.startMilliseconds, 1});
		events.push_back({sample.endMilliseconds, -1});
		if(sample.workerIndex == JobSystem::InvalidWorkerIndex){
			mainLaneUsed = true;
		} else {
			workers.insert(sample.workerIndex);
		}
	}

	std::sort(
		events.begin(),
		events.end(),
		[](const Event& lhs, const Event& rhs){
			if(lhs.time != rhs.time){
				return lhs.time < rhs.time;
			}
			return lhs.delta < rhs.delta;
		}
	);

	int active = 0;
	size_t peak = 0;
	for(const Event& event : events){
		active += event.delta;
		peak = (std::max)(peak, static_cast<size_t>((std::max)(active, 0)));
	}

	return {peak, workers.size(), mainLaneUsed};
}

inline ImU32 LaneColor(const SystemTaskProfileSample& sample){
	if(!sample.succeeded){
		return IM_COL32(210, 70, 70, 255);
	}
	if(sample.workerIndex == JobSystem::InvalidWorkerIndex){
		return ImGui::GetColorU32(ImGuiCol_Button);
	}

	const float hue = std::fmod(
		static_cast<float>(sample.workerIndex) * 0.173f,
		1.0f
	);
	return ImGui::ColorConvertFloat4ToU32(
		ImColor::HSV(hue, 0.58f, 0.90f, 1.0f)
	);
}

inline void DrawTimelineRuler(float timelineWidth, double totalMilliseconds){
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float height = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
	ImGui::InvisibleButton("##TimelineRuler", ImVec2(timelineWidth, height));

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImU32 gridColor = ImGui::GetColorU32(ImGuiCol_Border);
	for(int division = 0; division <= 4; ++division){
		const float ratio = static_cast<float>(division) / 4.0f;
		const float x = origin.x + timelineWidth * ratio;
		drawList->AddLine(
			ImVec2(x, origin.y),
			ImVec2(x, origin.y + height),
			gridColor
		);

		const double milliseconds = totalMilliseconds * ratio;
		char label[64]{};
		snprintf(label, sizeof(label), "%.3f ms", milliseconds);
		drawList->AddText(
			ImVec2(x + 3.0f, origin.y + 1.0f),
			ImGui::GetColorU32(ImGuiCol_TextDisabled),
			label
		);
	}
}

inline void DrawGantt(
	const SystemScheduleProfileSnapshot& snapshot,
	float pixelsPerMillisecond
){
	if(snapshot.samples.empty()){
		ImGui::TextDisabled("No task samples were captured for this domain.");
		return;
	}

	double totalMilliseconds = snapshot.totalMilliseconds;
	for(const SystemTaskProfileSample& sample : snapshot.samples){
		totalMilliseconds = (std::max)(totalMilliseconds, sample.endMilliseconds);
	}
	totalMilliseconds = (std::max)(totalMilliseconds, 0.001);

	const float minimumTimelineWidth = 620.0f;
	const float effectivePixelsPerMillisecond = (std::max)(
		pixelsPerMillisecond,
		minimumTimelineWidth / static_cast<float>(totalMilliseconds)
	);
	const float timelineWidth = static_cast<float>(totalMilliseconds) *
		effectivePixelsPerMillisecond;

	const ImGuiTableFlags flags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_ScrollX |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	if(!ImGui::BeginTable(
		"ScheduleGantt",
		3,
		flags,
		ImVec2(0.0f, 430.0f)
	)){
		return;
	}

	ImGui::TableSetupColumn("Task", ImGuiTableColumnFlags_WidthFixed, 230.0f);
	ImGui::TableSetupColumn("Actual Thread", ImGuiTableColumnFlags_WidthFixed, 92.0f);
	ImGui::TableSetupColumn(
		"Timeline",
		ImGuiTableColumnFlags_WidthFixed,
		timelineWidth
	);
	ImGui::TableSetupScrollFreeze(2, 1);
	ImGui::TableHeadersRow();

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::TextDisabled(
		"Capture #%llu",
		static_cast<unsigned long long>(snapshot.captureIndex)
	);
	ImGui::TableSetColumnIndex(1);
	ImGui::TextDisabled("%zu workers", snapshot.workerCount);
	ImGui::TableSetColumnIndex(2);
	DrawTimelineRuler(timelineWidth, totalMilliseconds);

	for(size_t index = 0; index < snapshot.samples.size(); ++index){
		const SystemTaskProfileSample& sample = snapshot.samples[index];
		ImGui::PushID(static_cast<int>(index));
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(sample.taskName.c_str());
		if(ImGui::IsItemHovered()){
			ImGui::BeginTooltip();
			ImGui::Text("Phase: %s", PhaseLabel(sample.phase));
			ImGui::Text("Priority: %d", sample.priority);
			ImGui::Text("Declared: %s", AffinityLabel(sample.declaredAffinity));
			ImGui::EndTooltip();
		}

		ImGui::TableSetColumnIndex(1);
		const std::string lane = LaneLabel(sample.workerIndex);
		ImGui::TextUnformatted(lane.c_str());

		ImGui::TableSetColumnIndex(2);
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + 6.0f;
		ImGui::InvisibleButton("##TimelineRow", ImVec2(timelineWidth, rowHeight));

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImU32 gridColor = ImGui::GetColorU32(ImGuiCol_Border, 0.45f);
		for(int division = 0; division <= 4; ++division){
			const float x = origin.x + timelineWidth *
				(static_cast<float>(division) / 4.0f);
			drawList->AddLine(
				ImVec2(x, origin.y),
				ImVec2(x, origin.y + rowHeight),
				gridColor
			);
		}

		const float startX = origin.x + static_cast<float>(
			sample.startMilliseconds * effectivePixelsPerMillisecond
		);
		const float endX = origin.x + static_cast<float>(
			sample.endMilliseconds * effectivePixelsPerMillisecond
		);
		const float visibleEndX = (std::max)(endX, startX + 2.0f);
		const ImVec2 barMin(startX, origin.y + 3.0f);
		const ImVec2 barMax(visibleEndX, origin.y + rowHeight - 3.0f);
		drawList->AddRectFilled(barMin, barMax, LaneColor(sample), 3.0f);

		if(visibleEndX - startX > 52.0f){
			char duration[48]{};
			snprintf(
				duration,
				sizeof(duration),
				"%.3f ms",
				sample.durationMilliseconds
			);
			drawList->AddText(
				ImVec2(startX + 4.0f, origin.y + 3.0f),
				IM_COL32(255, 255, 255, 255),
				duration
			);
		}

		if(ImGui::IsMouseHoveringRect(barMin, barMax)){
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(sample.taskName.c_str());
			ImGui::Separator();
			ImGui::Text("Actual thread: %s", lane.c_str());
			ImGui::Text("Declared affinity: %s", AffinityLabel(sample.declaredAffinity));
			ImGui::Text("Start: %.4f ms", sample.startMilliseconds);
			ImGui::Text("End: %.4f ms", sample.endMilliseconds);
			ImGui::Text("Duration: %.4f ms", sample.durationMilliseconds);
			ImGui::Text("Result: %s", sample.succeeded ? "Success" : "Failed");
			ImGui::EndTooltip();
		}

		ImGui::PopID();
	}

	ImGui::EndTable();
}

inline void DrawCompiledGraph(
	const CompiledSystemSchedule& schedule,
	const std::vector<SystemTask>& tasks
){
	if(!ImGui::CollapsingHeader("Compiled Dependency Graph")){
		return;
	}

	if(!schedule.valid){
		ImGui::TextColored(
			ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
			"Invalid schedule: %s",
			schedule.error.c_str()
		);
		return;
	}

	if(ImGui::BeginTable(
		"CompiledScheduleTable",
		6,
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_SizingStretchProp
	)){
		ImGui::TableSetupColumn("Task");
		ImGui::TableSetupColumn("System");
		ImGui::TableSetupColumn("Phase");
		ImGui::TableSetupColumn("Priority");
		ImGui::TableSetupColumn("Affinity");
		ImGui::TableSetupColumn("Dependencies");
		ImGui::TableHeadersRow();

		for(const SystemScheduleNode& node : schedule.nodes){
			const SystemTask& task = tasks[node.taskIndex];
			const char* systemName = task.owner
				? task.owner->GetSystemName()
				: nullptr;

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(task.name.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(systemName ? systemName : "Unknown");
			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted(PhaseLabel(task.order.phase));
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%d", task.order.priority);
			ImGui::TableSetColumnIndex(4);
			ImGui::TextUnformatted(AffinityLabel(task.threadAffinity));
			ImGui::TableSetColumnIndex(5);
			ImGui::Text("%zu", node.dependencies.size());
		}

		ImGui::EndTable();
	}
}

inline void CaptureFrozenSnapshots(
	SystemScheduleProfiler& profiler,
	ScheduleProfilerViewState& state
){
	for(size_t index = 0; index < state.frozenSnapshots.size(); ++index){
		state.frozenSnapshots[index] = profiler.GetLatestSnapshot(
			static_cast<SystemTaskDomain>(index)
		);
	}
}

inline void DrawDomain(
	SystemRegistry& registry,
	ScheduleProfilerViewState& state,
	SystemTaskDomain domain
){
	const std::vector<SystemTask>& tasks = registry.GetTasks();
	const CompiledSystemSchedule& schedule = registry.GetSchedule(domain);
	SystemScheduleProfiler& profiler = registry.GetScheduleProfiler();

	size_t anyWorkerTasks = 0;
	size_t mainThreadTasks = 0;
	for(const SystemScheduleNode& node : schedule.nodes){
		const SystemTask& task = tasks[node.taskIndex];
		if(task.threadAffinity == ThreadAffinity::AnyWorker){
			++anyWorkerTasks;
		} else {
			++mainThreadTasks;
		}
	}

	const size_t plannedWidth = CalculateDependencyLayerWidth(
		schedule,
		tasks,
		false
	);
	const size_t plannedWorkerWidth = CalculateDependencyLayerWidth(
		schedule,
		tasks,
		true
	);

	ImGui::Text("Tasks: %zu", schedule.nodes.size());
	ImGui::SameLine();
	ImGui::Text("Worker-capable: %zu", anyWorkerTasks);
	ImGui::SameLine();
	ImGui::Text("Main/Render: %zu", mainThreadTasks);
	ImGui::Text("Dependency edges: %zu", CountDependencyEdges(schedule));
	ImGui::SameLine();
	ImGui::Text("Max graph layer width: %zu", plannedWidth);
	ImGui::SameLine();
	ImGui::Text("Worker layer width: %zu", plannedWorkerWidth);

	std::optional<SystemScheduleProfileSnapshot> snapshot;
	const size_t domainIndex = static_cast<size_t>(domain);
	if(state.freeze){
		snapshot = state.frozenSnapshots[domainIndex];
	} else {
		snapshot = profiler.GetLatestSnapshot(domain);
	}

	if(!snapshot){
		ImGui::TextDisabled(
			"No completed %s schedule capture is available yet.",
			DomainLabel(domain)
		);
		DrawCompiledGraph(schedule, tasks);
		return;
	}

	const ActualConcurrencyInfo concurrency = CalculateActualConcurrency(*snapshot);
	const bool actualParallel = concurrency.peakConcurrency > 1;

	if(!snapshot->jobSystemRunning){
		ImGui::TextColored(
			ImVec4(1.0f, 0.65f, 0.25f, 1.0f),
			"Serial fallback: JobSystem was not running."
		);
	} else if(actualParallel){
		ImGui::TextColored(
			ImVec4(0.35f, 0.90f, 0.45f, 1.0f),
			"Parallel execution observed: peak %zu simultaneous tasks.",
			concurrency.peakConcurrency
		);
	} else if(anyWorkerTasks == 0){
		ImGui::TextDisabled("No AnyWorker tasks exist in this schedule.");
	} else {
		ImGui::TextColored(
			ImVec4(1.0f, 0.72f, 0.28f, 1.0f),
			"Worker tasks exist, but no overlap was observed in the latest capture."
		);
	}

	ImGui::Text(
		"Measured total: %.4f ms | Peak concurrency: %zu | Worker lanes used: %zu%s",
		snapshot->totalMilliseconds,
		concurrency.peakConcurrency,
		concurrency.workerLaneCount,
		concurrency.mainLaneUsed ? " + Main" : ""
	);

	ImGui::TextDisabled(
		"Bars represent Scheduler tasks. Internal ParallelFor child jobs are included in the parent task duration."
	);

	DrawGantt(*snapshot, state.pixelsPerMillisecond);
	DrawCompiledGraph(schedule, tasks);
}

inline void Draw(SystemRegistry& registry, ScheduleProfilerViewState& state){
	SystemScheduleProfiler& profiler = registry.GetScheduleProfiler();

	bool profilingEnabled = profiler.IsEnabled();
	if(ImGui::Checkbox("Capture Task Timings", &profilingEnabled)){
		profiler.SetEnabled(profilingEnabled);
	}
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip(
			"Records task start/end time and actual worker index.\n"
			"Disable this when profiling overhead must be completely avoided."
		);
	}

	ImGui::SameLine();
	const bool wasFrozen = state.freeze;
	if(ImGui::Checkbox("Freeze", &state.freeze)){
		if(state.freeze && !wasFrozen){
			CaptureFrozenSnapshots(profiler, state);
		} else if(!state.freeze){
			for(auto& snapshot : state.frozenSnapshots){
				snapshot.reset();
			}
		}
	}

	ImGui::SameLine();
	if(ImGui::Button("Clear Captures")){
		profiler.Clear();
		for(auto& snapshot : state.frozenSnapshots){
			snapshot.reset();
		}
	}

	ImGui::SetNextItemWidth(220.0f);
	ImGui::SliderFloat(
		"Timeline Scale (px/ms)",
		&state.pixelsPerMillisecond,
		20.0f,
		1200.0f,
		"%.0f"
	);

	ImGui::Text(
		"JobSystem: %s | Worker count: %zu",
		registry.GetJobSystem().IsRunning() ? "Running" : "Stopped",
		registry.GetJobSystem().WorkerCount()
	);

	if(ImGui::BeginTabBar("ScheduleDomainTabs")){
		constexpr std::array<SystemTaskDomain, 4> domains = {
			SystemTaskDomain::Fixed,
			SystemTaskDomain::Frame,
			SystemTaskDomain::Editor,
			SystemTaskDomain::Render
		};

		for(SystemTaskDomain domain : domains){
			if(ImGui::BeginTabItem(DomainLabel(domain))){
				DrawDomain(registry, state, domain);
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}
}

} // namespace ScheduleProfilerView
