// =======================================================================
//
// ScheduleProfileYamlExportUI.h
//
// =======================================================================
#pragma once

#include <string>

#include "Backends/ImGui/imgui.h"
#include "Editor/UI/ScheduleProfileYamlExporter.h"
#include "Editor/UI/ScheduleProfilerView.h"

namespace ScheduleProfileYamlExportUI {

inline void Draw(
	SystemRegistry& registry,
	const ScheduleProfilerViewState& viewState
) {
	static std::string lastExportPath;
	static std::string lastError;

	const ScheduleProfileYamlExporter::SnapshotSet snapshots =
		viewState.freeze
			? viewState.frozenSnapshots
			: ScheduleProfileYamlExporter::CollectLatestSnapshots(
				registry.GetScheduleProfiler()
			);

	const bool hasCapture =
		ScheduleProfileYamlExporter::HasAnySnapshot(snapshots);
	if(!hasCapture) {
		ImGui::BeginDisabled();
	}

	const char* label = viewState.freeze
		? "Export Frozen Captures to YAML"
		: "Export Latest Captures to YAML";
	if(ImGui::Button(
		label,
		ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)
	)) {
		const ScheduleProfileYamlExporter::ExportResult result =
			ScheduleProfileYamlExporter::Export(
				registry,
				snapshots,
				viewState.freeze
			);

		if(result.succeeded) {
			lastExportPath = result.outputPath.string();
			lastError.clear();
		} else {
			lastExportPath.clear();
			lastError = result.error;
		}
	}

	if(!hasCapture) {
		ImGui::EndDisabled();
		ImGui::TextDisabled(
			"A completed schedule capture is required before YAML export."
		);
	}

	if(!lastError.empty()) {
		ImGui::TextColored(
			ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
			"YAML export failed: %s",
			lastError.c_str()
		);
	} else if(!lastExportPath.empty()) {
		ImGui::TextWrapped("YAML saved: %s", lastExportPath.c_str());
	}

	ImGui::TextDisabled(
		"Output directory: Logs/Schedule"
	);
	ImGui::Separator();
}

} // namespace ScheduleProfileYamlExportUI
