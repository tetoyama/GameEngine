// =======================================================================
//
// PerformanceMonitorDashboardWidgets.h
// Presentation-only helpers for PerformanceMonitor.
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cfloat>
#include <cstdio>

#include <ImGui/imgui.h>

namespace PerformanceMonitorDashboardWidgets {

inline float Average(const float* samples, int count, bool ignoreZero = false){
	float total = 0.0f;
	int validCount = 0;
	for(int index = 0; index < count; ++index){
		if(ignoreZero && samples[index] <= 0.0f) continue;
		total += samples[index];
		++validCount;
	}
	return validCount > 0 ? total / static_cast<float>(validCount) : 0.0f;
}

inline float Maximum(const float* samples, int count){
	float maximum = 0.0f;
	for(int index = 0; index < count; ++index){
		maximum = (std::max)(maximum, samples[index]);
	}
	return maximum;
}

inline ImVec4 LoadColor(float ratio){
	if(ratio >= 2.0f) return ImVec4(1.00f, 0.34f, 0.32f, 1.0f);
	if(ratio >= 1.0f) return ImVec4(1.00f, 0.68f, 0.25f, 1.0f);
	return ImVec4(0.42f, 0.78f, 1.00f, 1.0f);
}

inline void MetricCard(
	const char* id,
	const char* label,
	const char* value,
	const char* detail,
	float loadRatio
){
	ImGui::PushID(id);
	const ImGuiStyle& style = ImGui::GetStyle();
	ImGui::PushStyleColor(
		ImGuiCol_ChildBg,
		ImVec4(
			style.Colors[ImGuiCol_FrameBg].x,
			style.Colors[ImGuiCol_FrameBg].y,
			style.Colors[ImGuiCol_FrameBg].z,
			0.72f
		)
	);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
	if(ImGui::BeginChild("##Card", ImVec2(0.0f, 70.0f), true, ImGuiWindowFlags_NoScrollbar)){
		ImGui::TextDisabled("%s", label);
		ImGui::PushStyleColor(ImGuiCol_Text, LoadColor(loadRatio));
		ImGui::SetWindowFontScale(1.18f);
		ImGui::TextUnformatted(value);
		ImGui::SetWindowFontScale(1.0f);
		ImGui::PopStyleColor();
		ImGui::TextDisabled("%s", detail);
	}
	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
	ImGui::PopID();
}

inline void BudgetPlot(
	const char* label,
	const char* id,
	const float* samples,
	int sampleCount,
	float current,
	float average,
	float targetMilliseconds
){
	char overlay[96]{};
	std::snprintf(
		overlay,
		sizeof(overlay),
		"Current %.2f ms   Avg %.2f ms",
		current,
		average
	);
	ImGui::TextUnformatted(label);

	const float plotMaximum = (std::max)(
		targetMilliseconds * 2.0f,
		Maximum(samples, sampleCount) * 1.10f
	);
	ImGui::PlotLines(
		id,
		samples,
		sampleCount,
		0,
		overlay,
		0.0f,
		plotMaximum,
		ImVec2(-1.0f, 58.0f)
	);

	const ImVec2 minimum = ImGui::GetItemRectMin();
	const ImVec2 maximum = ImGui::GetItemRectMax();
	const float ratio = plotMaximum > 0.0f
		? targetMilliseconds / plotMaximum
		: 0.0f;
	const float lineY = maximum.y - (maximum.y - minimum.y) * ratio;
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddLine(
		ImVec2(minimum.x, lineY),
		ImVec2(maximum.x, lineY),
		ImGui::GetColorU32(ImVec4(1.0f, 0.72f, 0.24f, 0.68f)),
		1.0f
	);
}

} // namespace PerformanceMonitorDashboardWidgets
