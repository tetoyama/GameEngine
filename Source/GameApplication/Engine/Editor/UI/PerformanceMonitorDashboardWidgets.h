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
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
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

inline float Percentile(
	const float* samples,
	int count,
	float percentile,
	bool ignoreZero = false
){
	if(!samples || count <= 0){
		return 0.0f;
	}

	// Dashboard histories are currently 60 samples. Keep a bounded stack
	// workspace so percentile inspection never allocates while the profiler is
	// measuring the frame. If a future caller supplies more data, prefer the
	// most recent 512 samples because the dashboard represents recent health.
	constexpr std::size_t MaxPercentileSamples = 512;
	std::array<float, MaxPercentileSamples> values{};
	const int firstSample = (std::max)(
		0,
		count - static_cast<int>(MaxPercentileSamples)
	);
	std::size_t validCount = 0;
	for(int index = firstSample; index < count; ++index){
		if(ignoreZero && samples[index] <= 0.0f) continue;
		values[validCount++] = samples[index];
	}
	if(validCount == 0){
		return 0.0f;
	}

	std::sort(values.begin(), values.begin() + validCount);
	percentile = (std::max)(0.0f, (std::min)(1.0f, percentile));
	const float position = percentile * static_cast<float>(validCount - 1);
	const std::size_t lower = static_cast<std::size_t>(std::floor(position));
	const std::size_t upper = static_cast<std::size_t>(std::ceil(position));
	if(lower == upper){
		return values[lower];
	}
	const float fraction = position - static_cast<float>(lower);
	return values[lower] + (values[upper] - values[lower]) * fraction;
}

inline int CountAbove(const float* samples, int count, float threshold){
	if(!samples || count <= 0 || threshold <= 0.0f){
		return 0;
	}
	int result = 0;
	for(int index = 0; index < count; ++index){
		if(samples[index] > threshold){
			++result;
		}
	}
	return result;
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

inline float MetricCardHeight(){
	const float lineHeight = ImGui::GetTextLineHeight();
	return (std::max)(72.0f, lineHeight * 3.0f + 26.0f);
}

inline void AddClippedText(
	ImDrawList* drawList,
	const ImVec2& position,
	ImU32 color,
	const char* text,
	const ImVec4& clip
){
	drawList->AddText(
		nullptr,
		0.0f,
		position,
		color,
		text,
		nullptr,
		0.0f,
		&clip
	);
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
	const float width = PositiveExtent(ImGui::GetContentRegionAvail().x);
	const float height = MetricCardHeight();
	const ImVec2 minimum = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##Metric", ImVec2(width, height));
	const bool hovered = ImGui::IsItemHovered();
	const ImVec2 maximum(minimum.x + width, minimum.y + height);
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	drawList->AddRectFilled(
		minimum,
		maximum,
		ImGui::GetColorU32(
			WithAlpha(style.Colors[ImGuiCol_FrameBg], hovered ? 0.68f : 0.54f)
		),
		6.0f
	);
	drawList->AddRect(
		minimum,
		maximum,
		ImGui::GetColorU32(
			WithAlpha(style.Colors[ImGuiCol_Border], hovered ? 0.44f : 0.30f)
		),
		6.0f,
		0,
		1.0f
	);

	const float paddingX = 10.0f;
	const float paddingY = 8.0f;
	const float lineHeight = ImGui::GetTextLineHeight();
	const float lineGap = 4.0f;
	const ImVec4 clip(
		minimum.x + paddingX,
		minimum.y + 1.0f,
		maximum.x - paddingX,
		maximum.y - 1.0f
	);
	const float labelY = minimum.y + paddingY;
	const float valueY = labelY + lineHeight + lineGap;
	const float detailY = valueY + lineHeight + lineGap;

	AddClippedText(
		drawList,
		ImVec2(minimum.x + paddingX, labelY),
		ImGui::GetColorU32(style.Colors[ImGuiCol_TextDisabled]),
		label,
		clip
	);
	AddClippedText(
		drawList,
		ImVec2(minimum.x + paddingX, valueY),
		ImGui::GetColorU32(LoadColor(loadRatio)),
		value,
		clip
	);
	AddClippedText(
		drawList,
		ImVec2(minimum.x + paddingX, detailY),
		ImGui::GetColorU32(style.Colors[ImGuiCol_TextDisabled]),
		detail,
		clip
	);

	const float meterMinimumX = minimum.x + paddingX;
	const float meterMaximumX = maximum.x - paddingX;
	const float meterY = maximum.y - 5.0f;
	drawList->AddLine(
		ImVec2(meterMinimumX, meterY),
		ImVec2(meterMaximumX, meterY),
		ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_Border], 0.32f)),
		2.0f
	);
	const float visibleLoad = (std::max)(0.0f, (std::min)(1.0f, loadRatio));
	if(visibleLoad > 0.0f){
		drawList->AddLine(
			ImVec2(meterMinimumX, meterY),
			ImVec2(
				meterMinimumX + (meterMaximumX - meterMinimumX) * visibleLoad,
				meterY
			),
			ImGui::GetColorU32(WithAlpha(LoadColor(loadRatio), 0.86f)),
			2.0f
		);
	}

	if(hovered){
		ImGui::SetTooltip("%s\n%s\n%s", label, value, detail);
	}
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
	const bool ignoreZeroForStatistics = targetValue <= 0.0f;
	const float peak = Maximum(samples, sampleCount);
	const float p95 = Percentile(
		samples,
		sampleCount,
		0.95f,
		ignoreZeroForStatistics
	);
	const int overTargetCount = CountAbove(samples, sampleCount, targetValue);
	char valueText[128]{};
	std::snprintf(
		valueText,
		sizeof(valueText),
		"%.*f %s  ·  avg %.*f  ·  P95 %.*f",
		precision,
		current,
		unit,
		precision,
		average,
		precision,
		p95
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
	const bool plotHovered = ImGui::IsItemHovered();
	const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(
		minimum,
		maximum,
		ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_FrameBg], 0.38f)),
		4.0f
	);

	const float robustReference = (std::max)(p95, current);
	const float plotMaximum = (std::max)(
		targetValue > 0.0f ? targetValue * 1.10f : 1.0f,
		robustReference > 0.0f ? robustReference * 1.25f : peak * 1.10f
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
		const ImU32 normalLineColor = ImGui::GetColorU32(
			WithAlpha(style.Colors[ImGuiCol_PlotLines], 0.88f)
		);
		const ImU32 overBudgetLineColor = ImGui::GetColorU32(
			ImVec4(1.0f, 0.68f, 0.25f, 0.90f)
		);
		const ImU32 clippedMarkerColor = ImGui::GetColorU32(
			ImVec4(1.0f, 0.34f, 0.32f, 0.95f)
		);
		ImVec2 previous = minimum;
		float previousValue = samples[0];
		for(int index = 0; index < sampleCount; ++index){
			const float sample = samples[index];
			const float x = minimum.x + width *
				(static_cast<float>(index) / static_cast<float>(sampleCount - 1));
			const float normalized = (std::max)(
				0.0f,
				(std::min)(1.0f, sample / plotMaximum)
			);
			const ImVec2 point(x, maximum.y - normalized * height);
			if(index > 0){
				const bool overBudget = targetValue > 0.0f &&
					(sample > targetValue || previousValue > targetValue);
				drawList->AddLine(
					previous,
					point,
					overBudget ? overBudgetLineColor : normalLineColor,
					1.25f
				);
			}
			if(sample > plotMaximum){
				drawList->AddCircleFilled(
					ImVec2(x, minimum.y + 2.0f),
					2.0f,
					clippedMarkerColor
				);
			}
			previous = point;
			previousValue = sample;
		}
	}

	if(plotHovered && sampleCount > 0){
		const float width = (std::max)(1.0f, maximum.x - minimum.x);
		const float height = maximum.y - minimum.y;
		const float localX = ImGui::GetIO().MousePos.x - minimum.x;
		const int index = (std::max)(
			0,
			(std::min)(
				sampleCount - 1,
				static_cast<int>(std::round(localX / width * (sampleCount - 1)))
			)
		);
		const float sampleRatio = plotMaximum > 0.0f
			? (std::max)(0.0f, (std::min)(1.0f, samples[index] / plotMaximum))
			: 0.0f;
		const float sampleX = minimum.x + width *
			(static_cast<float>(index) / static_cast<float>((std::max)(1, sampleCount - 1)));
		const float sampleY = maximum.y - sampleRatio * height;
		drawList->AddLine(
			ImVec2(sampleX, minimum.y),
			ImVec2(sampleX, maximum.y),
			ImGui::GetColorU32(WithAlpha(style.Colors[ImGuiCol_Border], 0.58f)),
			1.0f
		);
		drawList->AddCircleFilled(
			ImVec2(sampleX, sampleY),
			2.5f,
			ImGui::GetColorU32(style.Colors[ImGuiCol_PlotLinesHovered])
		);

		if(targetValue > 0.0f){
			ImGui::SetTooltip(
				"Sample %d\n%.*f %s%s\nAverage %.*f %s\nP95 %.*f %s\nPeak %.*f %s\nOver budget %d / %d",
				index,
				precision,
				samples[index],
				unit,
				samples[index] > targetValue ? " · over budget" : "",
				precision,
				average,
				unit,
				precision,
				p95,
				unit,
				precision,
				peak,
				unit,
				overTargetCount,
				sampleCount
			);
		}else{
			ImGui::SetTooltip(
				"Sample %d\n%.*f %s\nAverage %.*f %s\nP95 %.*f %s\nPeak %.*f %s",
				index,
				precision,
				samples[index],
				unit,
				precision,
				average,
				unit,
				precision,
				p95,
				unit,
				precision,
				peak,
				unit
			);
		}
	}
	ImGui::PopID();
}

} // namespace PerformanceMonitorDashboardWidgets
