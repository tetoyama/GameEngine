// =======================================================================
//
// PerformanceMonitorDashboardWidgets.h
// Presentation-only helpers for PerformanceMonitor.
//
// Design rules:
// - quiet material surfaces instead of stacked outlined cards
// - blue is an accent, not a section background
// - disclosure rows establish hierarchy without large filled headers
// - charts and bars carry information, not decoration
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cfloat>
#include <cmath>
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

inline ImVec4 WithAlpha(ImVec4 color, float alpha){
	color.w *= alpha;
	return color;
}

inline ImVec4 LoadColor(float ratio){
	const ImGuiStyle& style = ImGui::GetStyle();
	if(ratio >= 2.0f) return ImVec4(1.00f, 0.34f, 0.32f, 1.0f);
	if(ratio >= 1.0f) return ImVec4(1.00f, 0.68f, 0.25f, 1.0f);
	return style.Colors[ImGuiCol_Text];
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
	const ImVec4 surface = WithAlpha(style.Colors[ImGuiCol_FrameBg], 0.54f);
	const ImVec4 outline = WithAlpha(style.Colors[ImGuiCol_Border], 0.34f);

	ImGui::PushStyleColor(ImGuiCol_ChildBg, surface);
	ImGui::PushStyleColor(ImGuiCol_Border, outline);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 7.0f));
	if(ImGui::BeginChild(
		"##Metric",
		ImVec2(0.0f, 68.0f),
		true,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
	)){
		ImGui::TextDisabled("%s", label);
		ImGui::PushStyleColor(ImGuiCol_Text, LoadColor(loadRatio));
		ImGui::TextUnformatted(value);
		ImGui::PopStyleColor();
		ImGui::TextDisabled("%s", detail);
	}
	ImGui::EndChild();
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(2);
	ImGui::PopID();
}

inline bool SectionHeader(
	const char* id,
	const char* label,
	bool defaultOpen,
	const char* trailing = nullptr
){
	ImGui::PushID(id);
	ImGuiStorage* storage = ImGui::GetStateStorage();
	const ImGuiID openID = ImGui::GetID("##Open");
	bool open = storage->GetBool(openID, defaultOpen);

	const ImGuiStyle& style = ImGui::GetStyle();
	const float height = ImGui::GetFrameHeight() + 2.0f;
	const ImVec2 minimum = ImGui::GetCursorScreenPos();
	const ImVec2 size(ImGui::GetContentRegionAvail().x, height);
	ImGui::InvisibleButton("##Header", size);
	const bool hovered = ImGui::IsItemHovered();
	if(ImGui::IsItemClicked()){
		open = !open;
		storage->SetBool(openID, open);
	}

	const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if(hovered){
		drawList->AddRectFilled(
			minimum,
			maximum,
			ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_HeaderHovered], 0.20f)),
			4.0f
		);
	}

	const float centerY = (minimum.y + maximum.y) * 0.5f;
	const ImU32 arrowColor = ImGui::GetColorU32(style.Colors[ImGuiCol_TextDisabled]);
	if(open){
		drawList->AddTriangleFilled(
			ImVec2(minimum.x + 7.0f, centerY - 2.0f),
			ImVec2(minimum.x + 15.0f, centerY - 2.0f),
			ImVec2(minimum.x + 11.0f, centerY + 3.0f),
			arrowColor
		);
	}else{
		drawList->AddTriangleFilled(
			ImVec2(minimum.x + 8.0f, centerY - 4.0f),
			ImVec2(minimum.x + 8.0f, centerY + 4.0f),
			ImVec2(minimum.x + 13.0f, centerY),
			arrowColor
		);
	}

	const ImVec2 labelSize = ImGui::CalcTextSize(label);
	const float labelX = minimum.x + 23.0f;
	drawList->AddText(
		ImVec2(labelX, centerY - labelSize.y * 0.5f),
		ImGui::GetColorU32(style.Colors[ImGuiCol_Text]),
		label
	);
	if(trailing && trailing[0] != '\0'){
		const ImVec2 trailingSize = ImGui::CalcTextSize(trailing);
		const float trailingX = maximum.x - trailingSize.x - 5.0f;
		if(trailingX >= labelX + labelSize.x + 14.0f){
			drawList->AddText(
				ImVec2(trailingX, centerY - trailingSize.y * 0.5f),
				ImGui::GetColorU32(style.Colors[ImGuiCol_TextDisabled]),
				trailing
			);
		}
	}

	drawList->AddLine(
		ImVec2(minimum.x, maximum.y),
		ImVec2(maximum.x, maximum.y),
		ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_Border], 0.30f)),
		1.0f
	);
	ImGui::PopID();
	return open;
}

inline void ShareBar(float ratio, float height = 5.0f){
	ratio = (std::max)(0.0f, (std::min)(1.0f, ratio));
	const ImVec2 minimum = ImGui::GetCursorScreenPos();
	const ImVec2 size(ImGui::GetContentRegionAvail().x, height);
	ImGui::InvisibleButton("##Share", size);
	const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
	const ImGuiStyle& style = ImGui::GetStyle();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(
		minimum,
		maximum,
		ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_FrameBg], 0.66f)),
		height * 0.5f
	);
	if(ratio > 0.0f){
		drawList->AddRectFilled(
			minimum,
			ImVec2(minimum.x + size.x * ratio, maximum.y),
			ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_CheckMark], 0.72f)),
			height * 0.5f
		);
	}
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
	ImGui::PushID(id);
	const ImGuiStyle& style = ImGui::GetStyle();
	char valueText[96]{};
	std::snprintf(
		valueText,
		sizeof(valueText),
		"%.2f ms  ·  avg %.2f ms",
		current,
		average
	);

	ImGui::TextUnformatted(label);
	const ImVec2 valueSize = ImGui::CalcTextSize(valueText);
	const float valueX = ImGui::GetWindowContentRegionMax().x - valueSize.x;
	if(valueX > ImGui::GetCursorPosX() + 12.0f){
		ImGui::SameLine();
		ImGui::SetCursorPosX(valueX);
		ImGui::TextDisabled("%s", valueText);
	}

	const ImVec2 minimum = ImGui::GetCursorScreenPos();
	const ImVec2 size(ImGui::GetContentRegionAvail().x, 46.0f);
	ImGui::InvisibleButton("##Plot", size);
	const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(
		minimum,
		maximum,
		ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_FrameBg], 0.38f)),
		4.0f
	);

	const float plotMaximum = (std::max)(
		targetMilliseconds * 1.10f,
		Maximum(samples, sampleCount) * 1.10f
	);
	if(plotMaximum > 0.0f && targetMilliseconds > 0.0f){
		const float targetRatio = (std::min)(1.0f, targetMilliseconds / plotMaximum);
		const float targetY = maximum.y - size.y * targetRatio;
		drawList->AddLine(
			ImVec2(minimum.x, targetY),
			ImVec2(maximum.x, targetY),
			ImGui::GetColorU32(ImVec4(1.0f, 0.70f, 0.24f, 0.52f)),
			1.0f
		);
	}

	if(sampleCount > 1 && plotMaximum > 0.0f){
		const float width = maximum.x - minimum.x;
		const float height = maximum.y - minimum.y;
		const ImU32 lineColor = ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_PlotLines], 0.88f));
		ImVec2 previous = minimum;
		for(int index = 0; index < sampleCount; ++index){
			const float x = minimum.x + width *
				(static_cast<float>(index) / static_cast<float>(sampleCount - 1));
			const float normalized = (std::max)(
				0.0f,
				(std::min)(1.0f, samples[index] / plotMaximum)
			);
			const ImVec2 point(x, maximum.y - normalized * height);
			if(index > 0) drawList->AddLine(previous, point, lineColor, 1.25f);
			previous = point;
		}
	}

	if(ImGui::IsItemHovered() && sampleCount > 0){
		const float width = (std::max)(1.0f, maximum.x - minimum.x);
		const float localX = ImGui::GetIO().MousePos.x - minimum.x;
		const int index = (std::max)(
			0,
			(std::min)(
				sampleCount - 1,
				static_cast<int>(std::round(localX / width * (sampleCount - 1)))
			)
		);
		ImGui::SetTooltip("Sample %d\n%.4f ms", index, samples[index]);
	}
	ImGui::PopID();
}

} // namespace PerformanceMonitorDashboardWidgets
