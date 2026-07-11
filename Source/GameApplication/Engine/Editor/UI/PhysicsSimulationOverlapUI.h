// =======================================================================
//
// PhysicsSimulationOverlapUI.h
//
// Schedule ProfilerのFixed CaptureからPhysX Fetch待機の隠蔽状況を表示する。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <optional>

#include "Backends/ImGui/imgui.h"
#include "Editor/UI/ScheduleProfilerView.h"
#include "Scene/Registry/systemRegistry.h"
#include "System/Physic/PhysicsSimulationOverlapAnalysis.h"

namespace PhysicsSimulationOverlapUI {

inline void Draw(
	SystemRegistry& registry,
	const ScheduleProfilerViewState& state
){
	if(!ImGui::CollapsingHeader(
		"Physics Begin / Fetch Overlap",
		ImGuiTreeNodeFlags_DefaultOpen
	)){
		return;
	}

	std::optional<SystemScheduleProfileSnapshot> snapshot;
	const std::size_t fixedIndex =
		static_cast<std::size_t>(SystemTaskDomain::Fixed);
	if(state.freeze){
		snapshot = state.frozenSnapshots[fixedIndex];
	}else{
		snapshot = registry.GetScheduleProfiler().GetLatestSnapshot(
			SystemTaskDomain::Fixed
		);
	}

	if(!snapshot){
		ImGui::TextDisabled(
			"No completed Fixed schedule capture is available."
		);
		return;
	}

	const PhysicsSimulationOverlapAnalysis::Result result =
		PhysicsSimulationOverlapAnalysis::Analyze(*snapshot);
	if(!result.available){
		ImGui::TextDisabled(
			"Simulation.Simulate / Simulation.Fetch samples are unavailable."
		);
		return;
	}

	ImGui::Text(
		"Simulate submit: %.4f ms | Submit-to-fetch gap: %.4f ms",
		result.simulateMilliseconds,
		result.submissionGapMilliseconds
	);
	ImGui::Text(
		"Fetch: %.4f ms | Covered by other tasks: %.4f ms (%.1f%%)",
		result.fetchMilliseconds,
		result.overlappedFetchMilliseconds,
		result.coverageRatio * 100.0
	);
	ImGui::Text(
		"Uncovered fetch: %.4f ms | Overlapping tasks: %zu",
		result.uncoveredFetchMilliseconds,
		result.overlappingTaskCount
	);

	if(result.fetchNegligible){
		ImGui::TextColored(
			ImVec4(0.35f, 0.90f, 0.45f, 1.0f),
			"Keep current same-step Fetch: blocking cost is negligible."
		);
	}else if(result.overlapEffective){
		ImGui::TextColored(
			ImVec4(0.35f, 0.90f, 0.45f, 1.0f),
			"Keep current same-step Fetch: worker/main tasks cover most of the wait."
		);
	}else if(result.deeperPipeliningCandidate){
		ImGui::TextColored(
			ImVec4(1.0f, 0.72f, 0.28f, 1.0f),
			"Candidate for more pre-Fetch work. Do not defer Fetch across Fixed steps without latency validation."
		);
	}else{
		ImGui::TextDisabled(
			"Partial overlap observed; collect a frozen capture under representative load."
		);
	}

	ImGui::TextDisabled(
		"Coverage is the union of other Scheduler task intervals intersecting Simulation.Fetch."
	);
}

} // namespace PhysicsSimulationOverlapUI
