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

inline float PositiveExtent(float value){
	return (std::max)(1.0f, value);
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
	const float height = PositiveExtent(ImGui::GetFrameHeight() + 2.0f);
	const float width = PositiveExtent(ImGui::GetContentRegionAvail().x);
	const ImVec2 minimum = ImGui::GetCursorScreenPos();
	const ImVec2 size(width, height);
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

inline void DiagnosticListHeader(bool showAverage){
	const ImGuiStyle& style = ImGui::GetStyle();
	const ImVec2 minimum = ImGui::GetCursorScreenPos();
	const float width = PositiveExtent(ImGui::GetContentRegionAvail().x);
	const float height = 23.0f;
	ImGui::Dummy(ImVec2(width, height));
	const ImVec2 maximum(minimum.x + width, minimum.y + height);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImU32 muted = ImGui::GetColorU32(style.Colors[ImGuiCol_TextDisabled]);
	const float textY = minimum.y + 3.0f;
	const float averageRight = maximum.x - 4.0f;
	const float averageWidth = showAverage ? 82.0f : 0.0f;
	const float currentRight = averageRight - averageWidth;

	drawList->AddText(ImVec2(minimum.x + 4.0f, textY), muted, "Stage");
	const char* currentLabel = "Current";
	const float currentWidth = ImGui::CalcTextSize(currentLabel).x;
	drawList->AddText(
		ImVec2(currentRight - currentWidth, textY),
		muted,
		currentLabel
	);
	if(showAverage){
		const char* averageLabel = "Average";
		const float labelWidth = ImGui::CalcTextSize(averageLabel).x;
		drawList->AddText(
			ImVec2(averageRight - labelWidth, textY),
			muted,
			averageLabel
		);
	}
	drawList->AddLine(
		ImVec2(minimum.x, maximum.y - 1.0f),
		ImVec2(maximum.x, maximum.y - 1.0f),
		ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_Border], 0.34f)),
		1.0f
	);
}

inline void DiagnosticListRow(
	const char* id,
	const char* label,
	float current,
	float average,
	float ratio,
	bool showAverage,
	const char* unit = "ms",
	int precision = 3
){
	ImGui::PushID(id);
	ratio = (std::max)(0.0f, (std::min)(1.0f, ratio));
	const ImGuiStyle& style = ImGui::GetStyle();
	const ImVec2 minimum = ImGui::GetCursorScreenPos();
	const float width = PositiveExtent(ImGui::GetContentRegionAvail().x);
	const float height = 34.0f;
	ImGui::InvisibleButton("##DiagnosticRow", ImVec2(width, height));
	const bool hovered = ImGui::IsItemHovered();
	const ImVec2 maximum(minimum.x + width, minimum.y + height);
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	if(hovered){
		drawList->AddRectFilled(
			minimum,
			maximum,
			ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_HeaderHovered], 0.13f)),
			3.0f
		);
	}

	const float averageRight = maximum.x - 4.0f;
	const float averageColumnWidth = showAverage ? 82.0f : 0.0f;
	const float currentRight = averageRight - averageColumnWidth;
	const float labelRight = currentRight - 88.0f;
	const float textY = minimum.y + 6.0f;
	const ImVec4 clip(
		minimum.x + 4.0f,
		minimum.y,
		(std::max)(minimum.x + 4.0f, labelRight),
		maximum.y
	);
	drawList->AddText(
		nullptr,
		0.0f,
		ImVec2(minimum.x + 4.0f, textY),
		ImGui::GetColorU32(style.Colors[ImGuiCol_Text]),
		label,
		nullptr,
		0.0f,
		&clip
	);

	char currentText[48]{};
	std::snprintf(
		currentText,
		sizeof(currentText),
		"%.*f %s",
		precision,
		current,
		unit
	);
	const float currentWidth = ImGui::CalcTextSize(currentText).x;
	drawList->AddText(
		ImVec2(currentRight - currentWidth, textY),
		ImGui::GetColorU32(style.Colors[ImGuiCol_Text]),
		currentText
	);

	if(showAverage){
		char averageText[48]{};
		std::snprintf(
			averageText,
			sizeof(averageText),
			"%.*f %s",
			precision,
			average,
			unit
		);
		const float averageWidth = ImGui::CalcTextSize(averageText).x;
		drawList->AddText(
			ImVec2(averageRight - averageWidth, textY),
			ImGui::GetColorU32(style.Colors[ImGuiCol_TextDisabled]),
			averageText
		);
	}

	const float barY = maximum.y - 4.0f;
	const float barMinX = minimum.x + 4.0f;
	const float barMaxX = (std::max)(barMinX, maximum.x - 4.0f);
	drawList->AddLine(
		ImVec2(barMinX, barY),
		ImVec2(barMaxX, barY),
		ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_FrameBg], 0.82f)),
		2.0f
	);
	if(ratio > 0.0f){
		drawList->AddLine(
			ImVec2(barMinX, barY),
			ImVec2(barMinX + (barMaxX - barMinX) * ratio, barY),
			ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_CheckMark], 0.72f)),
			2.0f
		);
	}
	drawList->AddLine(
		ImVec2(minimum.x, maximum.y - 1.0f),
		ImVec2(maximum.x, maximum.y - 1.0f),
		ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_Border], 0.20f)),
		1.0f
	);

	if(hovered){
		if(showAverage){
			ImGui::SetTooltip(
				"%s\nCurrent %.*f %s\nAverage %.*f %s\nShare %.1f%%",
				label,
				precision,
				current,
				unit,
				precision,
				average,
				unit,
				ratio * 100.0f
			);
		}else{
			ImGui::SetTooltip(
				"%s\nCurrent %.*f %s\nShare %.1f%%",
				label,
				precision,
				current,
				unit,
				ratio * 100.0f
			);
		}
	}
	ImGui::PopID();
}

inline void BudgetPlot(
	const char* label,
	const char* id,
	const float* samples,
	int sampleCount,
	float current,
	float average,
	float targetValue,
	const char* unit = "ms",
	int precision = 2
){
	ImGui::PushID(id);
	const ImGuiStyle& style = ImGui::GetStyle();
	const float peak = Maximum(samples, sampleCount);
	char valueText[128]{};
	std::snprintf(
		valueText,
		sizeof(valueText),
		"%.*f %s  ·  avg %.*f  ·  peak %.*f",
		precision,
		current,
		unit,
		precision,
		average,
		precision,
		peak
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
	const ImVec2 size(
		PositiveExtent(ImGui::GetContentRegionAvail().x),
		46.0f
	);
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
		targetValue * 1.10f,
		peak * 1.10f
	);
	if(plotMaximum > 0.0f && targetValue > 0.0f){
		const float targetRatio = (std::min)(1.0f, targetValue / plotMaximum);
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
		const ImU32 lineColor = ImGui::GetColorU32(
			WithAlpha(style.Colors[ImGuiCol_PlotLines], 0.88f)
		);
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
		ImGui::SetTooltip(
			"Sample %d\n%.*f %s",
			index,
			precision,
			samples[index],
			unit
		);
	}
	ImGui::PopID();
}

} // namespace PerformanceMonitorDashboardWidgets
