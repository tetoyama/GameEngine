// =======================================================================
//
// PerformanceMonitor.cpp
//
// =======================================================================
#include "PerformanceMonitor.h"

#include <Psapi.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <iterator>
#include <utility>
#include <vector>

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"
#include "Editor/UI/PerformanceMonitorDashboardWidgets.h"
#include "sceneManager.h"
#include "Service/Graphics/graphicsContext.h"

namespace {

template<typename T, std::size_t N>
void ShiftSamples(T (&samples)[N]){
	for(std::size_t index = 0; index + 1 < N; ++index){
		samples[index] = samples[index + 1];
	}
}

template<typename T, std::size_t N>
void ShiftSamples(std::array<T, N>& samples){
	for(std::size_t index = 0; index + 1 < N; ++index){
		samples[index] = samples[index + 1];
	}
}

const char* GpuTimingStatusName(GpuFrameTimingStatus status){
	switch(status){
		case GpuFrameTimingStatus::Pending: return "Pending";
		case GpuFrameTimingStatus::Resolved: return "Resolved";
		case GpuFrameTimingStatus::Invalid: return "Invalid";
		case GpuFrameTimingStatus::Dropped: return "Dropped";
	}
	return "Unknown";
}

void DrawTimingTableRow(
	const char* label,
	float current,
	float average,
	float normalizationTotal
){
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::TextUnformatted(label);
	ImGui::TableSetColumnIndex(1);
	ImGui::Text("%.3f ms", current);
	ImGui::TableSetColumnIndex(2);
	ImGui::TextDisabled("%.3f ms", average);
	ImGui::TableSetColumnIndex(3);
	ImGui::PushID(label);
	PerformanceMonitorDashboardWidgets::ShareBar(
		normalizationTotal > 0.0f ? current / normalizationTotal : 0.0f
	);
	ImGui::PopID();
}

} // namespace

void PerformanceMonitor::RecordCompletedFrame(const EditorDrawContext& ctx){
	const uint64_t frameSerial = ctx.DrawTiming.frameSerial;
	if(frameSerial == 0 || frameSerial == LastRecordedFrameSerial){
		return;
	}
	if(frameSerial < LastRecordedFrameSerial){
		return;
	}
	LastRecordedFrameSerial = frameSerial;

	if(ctx.ResizeSerial != LastResizeSerial){
		LastResizeSerial = ctx.ResizeSerial;
		ResizeGraceFrames = 8;
	}

	FrameTimingRecord record{};
	record.frame = frameSerial;
	record.updateMilliseconds = static_cast<float>(ctx.UpdateTime * 1000.0);
	record.drawMilliseconds = static_cast<float>(ctx.DrawTime * 1000.0);
	record.drawTiming = ctx.DrawTiming;
	record.resizeMilliseconds = static_cast<float>(ctx.LastResizeCpuTime * 1000.0);
	record.startup = frameSerial <= 180;
	record.resize = ResizeGraceFrames > 0;

	if(ctx.EditorPanelTimings){
		for(const EditorPanelTiming& timing : *ctx.EditorPanelTimings){
			const float milliseconds = static_cast<float>(timing.seconds * 1000.0);
			if(milliseconds > record.dominantPanelMilliseconds){
				record.dominantPanelMilliseconds = milliseconds;
				record.dominantPanel = timing.name ? timing.name : "Unknown";
			}
		}
	}

	FrameHistory.push_back(record);
	if(FrameHistory.size() > 512){
		FrameHistory.pop_front();
	}

	ShiftSamples(UpdateSamples);
	ShiftSamples(DrawSamples);
	ShiftSamples(FramePacingWaitSamples);
	ShiftSamples(FrameSetupSamples);
	ShiftSamples(ImGuiBeginSamples);
	ShiftSamples(RenderScheduleSamples);
	ShiftSamples(DebugDrawSamples);
	ShiftSamples(EditorUIBuildSamples);
	ShiftSamples(ImGuiRenderSamples);
	ShiftSamples(PresentSamples);
	ShiftSamples(UnaccountedSamples);
	ShiftSamples(GPUFrameTimeSamples);
	ShiftSamples(FrameSerialSamples);
	ShiftSamples(GPUFrameStatusSamples);

	UpdateSamples[SAMPLE_LENGTH - 1] = record.updateMilliseconds;
	DrawSamples[SAMPLE_LENGTH - 1] = record.drawMilliseconds;
	FramePacingWaitSamples[SAMPLE_LENGTH - 1] =
		static_cast<float>(ctx.DrawTiming.framePacingWait * 1000.0);
	FrameSetupSamples[SAMPLE_LENGTH - 1] =
		static_cast<float>(ctx.DrawTiming.frameSetup * 1000.0);
	ImGuiBeginSamples[SAMPLE_LENGTH - 1] =
		static_cast<float>(ctx.DrawTiming.imguiBegin * 1000.0);
	RenderScheduleSamples[SAMPLE_LENGTH - 1] =
		static_cast<float>(ctx.DrawTiming.renderSchedule * 1000.0);
	DebugDrawSamples[SAMPLE_LENGTH - 1] =
		static_cast<float>(ctx.DrawTiming.debugDraw * 1000.0);
	EditorUIBuildSamples[SAMPLE_LENGTH - 1] =
		static_cast<float>(ctx.DrawTiming.editorUIBuild * 1000.0);
	ImGuiRenderSamples[SAMPLE_LENGTH - 1] =
		static_cast<float>(ctx.DrawTiming.imguiRender * 1000.0);
	PresentSamples[SAMPLE_LENGTH - 1] =
		static_cast<float>(ctx.DrawTiming.present * 1000.0);
	UnaccountedSamples[SAMPLE_LENGTH - 1] =
		static_cast<float>(ctx.DrawTiming.GetUnaccountedTime() * 1000.0);
	GPUFrameTimeSamples[SAMPLE_LENGTH - 1] = 0.0f;
	FrameSerialSamples[SAMPLE_LENGTH - 1] = frameSerial;
	GPUFrameStatusSamples[SAMPLE_LENGTH - 1] = GpuFrameTimingStatus::Pending;

	for(auto& series : PanelTimingSamples){
		ShiftSamples(series.samples);
		series.samples[SAMPLE_LENGTH - 1] = 0.0f;
	}
	if(ctx.EditorPanelTimings){
		for(const EditorPanelTiming& timing : *ctx.EditorPanelTimings){
			if(!timing.name) continue;
			auto it = std::find_if(
				PanelTimingSamples.begin(),
				PanelTimingSamples.end(),
				[&](const PanelTimingSampleSeries& series){
					return series.name == timing.name;
				}
			);
			if(it == PanelTimingSamples.end()){
				PanelTimingSamples.push_back({timing.name, {}});
				it = std::prev(PanelTimingSamples.end());
			}
			it->samples[SAMPLE_LENGTH - 1] =
				static_cast<float>(timing.seconds * 1000.0);
		}
	}

	if(ResizeGraceFrames > 0){
		--ResizeGraceFrames;
	}

	const auto deferred = DeferredGpuResults.find(frameSerial);
	if(deferred != DeferredGpuResults.end()){
		const GpuFrameTimingResult result = deferred->second;
		DeferredGpuResults.erase(deferred);
		ApplyGpuFrameTiming(result);
	}
}

void PerformanceMonitor::ApplyGpuFrameTiming(
	const GpuFrameTimingResult& result
){
	if(result.frameSerial == 0 || result.status == GpuFrameTimingStatus::Pending){
		return;
	}

	auto frame = std::find_if(
		FrameHistory.begin(),
		FrameHistory.end(),
		[&](const FrameTimingRecord& record){
			return record.frame == result.frameSerial;
		}
	);
	if(frame == FrameHistory.end()){
		if(FrameHistory.empty() || result.frameSerial > FrameHistory.back().frame){
			DeferredGpuResults[result.frameSerial] = result;
			while(DeferredGpuResults.size() > 64){
				DeferredGpuResults.erase(DeferredGpuResults.begin());
			}
		}
		return;
	}

	frame->gpuStatus = result.status;
	frame->gpuMilliseconds = result.status == GpuFrameTimingStatus::Resolved
		? static_cast<float>(result.seconds * 1000.0)
		: 0.0f;
	frame->gpuResolvedPassMask = result.resolvedPassMask;
	frame->gpuInvalidPassMask = result.invalidPassMask;
	for(std::size_t index = 0; index < GpuPassTimingScopeCount; ++index){
		const std::uint64_t bit = 1ull << index;
		frame->gpuPassMilliseconds[index] =
			(result.resolvedPassMask & bit) != 0
			? static_cast<float>(result.passSeconds[index] * 1000.0)
			: 0.0f;
	}

	for(std::size_t index = 0; index < SAMPLE_LENGTH; ++index){
		if(FrameSerialSamples[index] != result.frameSerial){
			continue;
		}
		GPUFrameStatusSamples[index] = result.status;
		GPUFrameTimeSamples[index] = frame->gpuMilliseconds;
		break;
	}
}

void PerformanceMonitor::ApplyGpuFrameTimings(const EditorDrawContext& ctx){
	if(!ctx.ResolvedGpuFrameTimings){
		return;
	}
	for(const GpuFrameTimingResult& result : *ctx.ResolvedGpuFrameTimings){
		ApplyGpuFrameTiming(result);
	}
}

void PerformanceMonitor::RebuildFrameSpikes(){
	FrameSpikes.clear();
	bool previousWasSpike = false;
	uint64_t previousSpikeFrame = 0;

	for(const FrameTimingRecord& frame : FrameHistory){
		const float gpuMs = frame.gpuStatus == GpuFrameTimingStatus::Resolved
			? frame.gpuMilliseconds
			: 0.0f;
		const float peakMs = (std::max)(
			frame.updateMilliseconds,
			(std::max)(frame.drawMilliseconds, gpuMs)
		);
		if(peakMs < SpikeThresholdMilliseconds){
			previousWasSpike = false;
			continue;
		}

		FrameSpikeRecord record{};
		record.frame = frame.frame;
		record.peakMilliseconds = peakMs;
		record.updateMilliseconds = frame.updateMilliseconds;
		record.drawMilliseconds = frame.drawMilliseconds;
		record.gpuMilliseconds = gpuMs;
		record.gpuStatus = frame.gpuStatus;
		record.framePacingMilliseconds =
			static_cast<float>(frame.drawTiming.framePacingWait * 1000.0);
		record.renderMilliseconds =
			static_cast<float>(frame.drawTiming.renderSchedule * 1000.0);
		record.editorMilliseconds =
			static_cast<float>(frame.drawTiming.editorUIBuild * 1000.0);
		record.presentMilliseconds =
			static_cast<float>(frame.drawTiming.present * 1000.0);
		record.unaccountedMilliseconds =
			static_cast<float>(frame.drawTiming.GetUnaccountedTime() * 1000.0);
		record.resizeMilliseconds = frame.resizeMilliseconds;
		record.dominantPanel = frame.dominantPanel;
		record.dominantPanelMilliseconds = frame.dominantPanelMilliseconds;
		record.startup = frame.startup;
		record.resize = frame.resize;

		auto considerDominant = [&](const char* name, float milliseconds){
			if(milliseconds > record.dominantMilliseconds){
				record.dominantMilliseconds = milliseconds;
				record.dominantSection = name;
			}
		};
		considerDominant("Update CPU", record.updateMilliseconds);
		considerDominant("Frame Pacing Wait", record.framePacingMilliseconds);
		considerDominant("Render Schedule CPU", record.renderMilliseconds);
		considerDominant("Editor UI CPU", record.editorMilliseconds);
		considerDominant("Present / Queue Wait", record.presentMilliseconds);
		considerDominant("Unaccounted Draw CPU", record.unaccountedMilliseconds);
		considerDominant("GPU Frame", record.gpuMilliseconds);

		const bool contiguous = previousWasSpike &&
			frame.frame == previousSpikeFrame + 1 &&
			!FrameSpikes.empty();
		if(contiguous){
			FrameSpikeRecord& active = FrameSpikes.back();
			active.startup = active.startup || record.startup;
			active.resize = active.resize || record.resize;
			active.resizeMilliseconds =
				(std::max)(active.resizeMilliseconds, record.resizeMilliseconds);
			if(record.peakMilliseconds > active.peakMilliseconds){
				const bool startup = active.startup;
				const bool resize = active.resize;
				const float resizeMs = active.resizeMilliseconds;
				active = std::move(record);
				active.startup = startup;
				active.resize = resize;
				active.resizeMilliseconds = resizeMs;
			}
		}else{
			FrameSpikes.push_back(std::move(record));
			if(FrameSpikes.size() > 32){
				FrameSpikes.pop_front();
			}
		}
		previousWasSpike = true;
		previousSpikeFrame = frame.frame;
	}
}

void PerformanceMonitor::Draw(const EditorDrawContext ctx){
	using namespace PerformanceMonitorDashboardWidgets;

	RecordCompletedFrame(ctx);
	ApplyGpuFrameTimings(ctx);
	RebuildFrameSpikes();

	const double fps = ctx.FPS;
	const double fixedFps = ctx.FixedUpdateFPS;
	bool* showPerformanceMonitor =
		&m_editor->GetUI<MenuBar>()->showPerformanceMonitor;

	PROCESS_MEMORY_COUNTERS_EX memory{};
	const BOOL memoryAvailable = GetProcessMemoryInfo(
		GetCurrentProcess(),
		reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
		sizeof(memory)
	);

	SampleCount = (SampleCount + 1) % TARGET_FPS;
	if(SampleCount == 0){
		ShiftSamples(FixedFpsSamples);
		ShiftSamples(DeltaFpsSamples);
		ShiftSamples(UsageSamples);
		ShiftSamples(CommitSizeSamples);
		ShiftSamples(WorkingSetSizeSamples);
		FixedFpsSamples[SAMPLE_LENGTH - 1] = static_cast<float>(fixedFps);
		DeltaFpsSamples[SAMPLE_LENGTH - 1] = static_cast<float>(fps);

		if(memoryAvailable){
			const SIZE_T usageBase = memory.WorkingSetSize + memory.PagefileUsage;
			UsageSamples[SAMPLE_LENGTH - 1] = usageBase > 0
				? 100.0f * static_cast<float>(memory.WorkingSetSize) /
					static_cast<float>(usageBase)
				: 0.0f;
			CommitSizeSamples[SAMPLE_LENGTH - 1] =
				memory.PagefileUsage / 1000000.0f;
			WorkingSetSizeSamples[SAMPLE_LENGTH - 1] =
				memory.WorkingSetSize / 1000000.0f;
		}
	}

	if(!showPerformanceMonitor || !*showPerformanceMonitor) return;

	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&windowClass);
	if(!ImGui::Begin("Performance Monitor", showPerformanceMonitor)){
		ImGui::End();
		return;
	}

	const float frameBudget = 1000.0f / 60.0f;
	const float updateCurrent = UpdateSamples[SAMPLE_LENGTH - 1];
	const float drawCurrent = DrawSamples[SAMPLE_LENGTH - 1];
	const float cpuCurrent = updateCurrent + drawCurrent;
	const float updateAverage = Average(UpdateSamples, SAMPLE_LENGTH);
	const float drawAverage = Average(DrawSamples, SAMPLE_LENGTH);

	const FrameTimingRecord* latest =
		FrameHistory.empty() ? nullptr : &FrameHistory.back();
	const FrameTimingRecord* latestResolved = nullptr;
	for(auto iterator = FrameHistory.rbegin(); iterator != FrameHistory.rend(); ++iterator){
		if(iterator->gpuStatus == GpuFrameTimingStatus::Resolved){
			latestResolved = &*iterator;
			break;
		}
	}
	const float gpuCurrent = latestResolved
		? latestResolved->gpuMilliseconds
		: 0.0f;
	const float gpuAverage = Average(GPUFrameTimeSamples, SAMPLE_LENGTH, true);
	const float workingSetMb = memoryAvailable
		? static_cast<float>(memory.WorkingSetSize / 1000000.0)
		: 0.0f;

	char value[64]{};
	char detail[96]{};
	const float summaryWidth = ImGui::GetContentRegionAvail().x;
	const int summaryColumns = summaryWidth >= 920.0f
		? 4
		: (summaryWidth >= 430.0f ? 2 : 1);
	if(ImGui::BeginTable(
		"PerformanceSummary",
		summaryColumns,
		ImGuiTableFlags_SizingStretchSame |
		ImGuiTableFlags_NoSavedSettings |
		ImGuiTableFlags_NoPadOuterX
	)){
		ImGui::TableNextColumn();
		std::snprintf(value, sizeof(value), "%.1f FPS", fps);
		std::snprintf(detail, sizeof(detail), "Fixed %.1f Hz", fixedFps);
		MetricCard(
			"FPS",
			"Frame Rate",
			value,
			detail,
			fps > 0.0 ? 60.0f / static_cast<float>(fps) : 2.0f
		);

		ImGui::TableNextColumn();
		std::snprintf(value, sizeof(value), "%.2f ms", cpuCurrent);
		std::snprintf(
			detail,
			sizeof(detail),
			"Update %.2f · Draw %.2f",
			updateCurrent,
			drawCurrent
		);
		MetricCard("CPU", "CPU Frame", value, detail, cpuCurrent / frameBudget);

		ImGui::TableNextColumn();
		if(latestResolved){
			std::snprintf(value, sizeof(value), "%.2f ms", gpuCurrent);
			std::snprintf(
				detail,
				sizeof(detail),
				"Frame %llu · Avg %.2f",
				static_cast<unsigned long long>(latestResolved->frame),
				gpuAverage
			);
		}else{
			std::snprintf(value, sizeof(value), "Pending");
			std::snprintf(detail, sizeof(detail), "Waiting for GPU query");
		}
		MetricCard("GPU", "GPU Frame", value, detail, gpuCurrent / frameBudget);

		ImGui::TableNextColumn();
		std::snprintf(value, sizeof(value), "%.0f MB", workingSetMb);
		std::snprintf(
			detail,
			sizeof(detail),
			"Commit %.0f MB",
			memoryAvailable ? memory.PagefileUsage / 1000000.0 : 0.0
		);
		MetricCard("Memory", "Working Set", value, detail, 0.0f);
		ImGui::EndTable();
	}

	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
	ImGui::TextWrapped(
		"VSync %s  ·  Tearing %s  ·  Pacing %s  ·  Timeouts %llu",
		ctx.VSyncEnabled ? "ON" : "OFF",
		ctx.TearingSupported ? "Supported" : "Unavailable",
		ctx.FrameLatencyWaitableObjectEnabled ? "Waitable Object" : "DXGI Fallback",
		static_cast<unsigned long long>(ctx.FrameLatencyWaitTimeoutCount)
	);
	ImGui::PopStyleColor();
	ImGui::Spacing();

	if(SectionHeader("FrameBudget", "Frame Budget", true, "60 FPS target")){
		ImGui::Indent(4.0f);
		BudgetPlot(
			"Update CPU",
			"UpdateBudget",
			UpdateSamples,
			SAMPLE_LENGTH,
			updateCurrent,
			updateAverage,
			frameBudget
		);
		BudgetPlot(
			"Draw CPU",
			"DrawBudget",
			DrawSamples,
			SAMPLE_LENGTH,
			drawCurrent,
			drawAverage,
			frameBudget
		);
		BudgetPlot(
			"GPU Frame",
			"GpuBudget",
			GPUFrameTimeSamples,
			SAMPLE_LENGTH,
			gpuCurrent,
			gpuAverage,
			frameBudget
		);
		ImGui::Unindent(4.0f);
		ImGui::Spacing();
	}

	if(SectionHeader("Breakdown", "CPU / GPU Breakdown", true)){
		ImGui::Indent(4.0f);
		if(ImGui::BeginTable(
			"DrawTimingBreakdown",
			4,
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_SizingStretchProp |
			ImGuiTableFlags_NoSavedSettings
		)){
			ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthStretch, 1.5f);
			ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 86.0f);
			ImGui::TableSetupColumn("Average", ImGuiTableColumnFlags_WidthFixed, 86.0f);
			ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableHeadersRow();
			DrawTimingTableRow("Frame Pacing Wait", FramePacingWaitSamples[SAMPLE_LENGTH - 1], Average(FramePacingWaitSamples, SAMPLE_LENGTH), drawCurrent);
			DrawTimingTableRow("Frame Setup", FrameSetupSamples[SAMPLE_LENGTH - 1], Average(FrameSetupSamples, SAMPLE_LENGTH), drawCurrent);
			DrawTimingTableRow("ImGui Begin", ImGuiBeginSamples[SAMPLE_LENGTH - 1], Average(ImGuiBeginSamples, SAMPLE_LENGTH), drawCurrent);
			DrawTimingTableRow("Render Schedule", RenderScheduleSamples[SAMPLE_LENGTH - 1], Average(RenderScheduleSamples, SAMPLE_LENGTH), drawCurrent);
			DrawTimingTableRow("Debug Draw", DebugDrawSamples[SAMPLE_LENGTH - 1], Average(DebugDrawSamples, SAMPLE_LENGTH), drawCurrent);
			DrawTimingTableRow("Editor UI Build", EditorUIBuildSamples[SAMPLE_LENGTH - 1], Average(EditorUIBuildSamples, SAMPLE_LENGTH), drawCurrent);
			DrawTimingTableRow("ImGui Render", ImGuiRenderSamples[SAMPLE_LENGTH - 1], Average(ImGuiRenderSamples, SAMPLE_LENGTH), drawCurrent);
			DrawTimingTableRow("Present / Queue Wait", PresentSamples[SAMPLE_LENGTH - 1], Average(PresentSamples, SAMPLE_LENGTH), drawCurrent);
			DrawTimingTableRow("Unaccounted Draw CPU", UnaccountedSamples[SAMPLE_LENGTH - 1], Average(UnaccountedSamples, SAMPLE_LENGTH), drawCurrent);
			ImGui::EndTable();
		}

		if(latest){
			ImGui::TextDisabled(
				"Latest GPU query · Frame %llu · %s",
				static_cast<unsigned long long>(latest->frame),
				GpuTimingStatusName(latest->gpuStatus)
			);
		}
		if(latestResolved && SectionHeader("GpuPasses", "GPU Passes", false)){
			std::vector<std::pair<std::size_t, float>> passes;
			float accounted = 0.0f;
			for(std::size_t index = 0; index < GpuPassTimingScopeCount; ++index){
				const std::uint64_t bit = 1ull << index;
				if((latestResolved->gpuResolvedPassMask & bit) == 0) continue;
				const float milliseconds = latestResolved->gpuPassMilliseconds[index];
				accounted += milliseconds;
				passes.emplace_back(index, milliseconds);
			}
			std::sort(
				passes.begin(),
				passes.end(),
				[](const auto& left, const auto& right){
					return left.second > right.second;
				}
			);

			if(ImGui::BeginTable(
				"GpuPassTable",
				3,
				ImGuiTableFlags_RowBg |
				ImGuiTableFlags_SizingStretchProp |
				ImGuiTableFlags_NoSavedSettings
			)){
				ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("Frame Share", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableHeadersRow();
				for(const auto& [index, milliseconds] : passes){
					ImGui::PushID(static_cast<int>(index));
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(
						GpuPassTimingScopeName(static_cast<GpuPassTimingScope>(index))
					);
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%.3f ms", milliseconds);
					ImGui::TableSetColumnIndex(2);
					ShareBar(gpuCurrent > 0.0f ? milliseconds / gpuCurrent : 0.0f);
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
			ImGui::TextDisabled(
				"Accounted %.3f ms · Unaccounted %.3f ms",
				accounted,
				(std::max)(0.0f, gpuCurrent - accounted)
			);
		}
		ImGui::Unindent(4.0f);
		ImGui::Spacing();
	}

	if(SectionHeader("EditorPanels", "Editor Panel CPU", false)){
		ImGui::Indent(4.0f);
		std::vector<const PanelTimingSampleSeries*> sortedPanels;
		sortedPanels.reserve(PanelTimingSamples.size());
		for(const PanelTimingSampleSeries& series : PanelTimingSamples){
			sortedPanels.push_back(&series);
		}
		std::sort(
			sortedPanels.begin(),
			sortedPanels.end(),
			[](const PanelTimingSampleSeries* left, const PanelTimingSampleSeries* right){
				return left->samples[SAMPLE_LENGTH - 1] >
					right->samples[SAMPLE_LENGTH - 1];
			}
		);

		if(ImGui::BeginTable(
			"EditorPanelTimingTable",
			3,
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_SizingStretchProp |
			ImGuiTableFlags_NoSavedSettings
		)){
			ImGui::TableSetupColumn("Panel", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableSetupColumn("Average", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableHeadersRow();
			for(const PanelTimingSampleSeries* series : sortedPanels){
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(series->name.c_str());
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.3f ms", series->samples[SAMPLE_LENGTH - 1]);
				ImGui::TableSetColumnIndex(2);
				ImGui::TextDisabled(
					"%.3f ms",
					Average(series->samples.data(), SAMPLE_LENGTH)
				);
			}
			ImGui::EndTable();
		}
		ImGui::Unindent(4.0f);
		ImGui::Spacing();
	}

	if(SectionHeader(
		"FrameSpikes",
		"Frame Spike Diagnostics",
		!FrameSpikes.empty(),
		FrameSpikes.empty() ? "No spikes" : nullptr
	)){
		ImGui::Indent(4.0f);
		const float actionWidth = 100.0f;
		const float sliderWidth = (std::max)(
			120.0f,
			ImGui::GetContentRegionAvail().x -
				actionWidth - ImGui::GetStyle().ItemSpacing.x
		);
		ImGui::SetNextItemWidth(sliderWidth);
		ImGui::SliderFloat(
			"##SpikeThreshold",
			&SpikeThresholdMilliseconds,
			5.0f,
			100.0f,
			"Threshold %.1f ms"
		);
		ImGui::SameLine();
		if(ImGui::Button("Clear History", ImVec2(actionWidth, 0.0f))){
			FrameHistory.clear();
			FrameSpikes.clear();
			DeferredGpuResults.clear();
		}
		ImGui::TextDisabled(
			"CPU and GPU samples are joined only by matching Frame Serial."
		);

		if(FrameSpikes.empty()){
			ImGui::TextDisabled("No frame spikes above the current threshold.");
		}else{
			for(auto iterator = FrameSpikes.rbegin(); iterator != FrameSpikes.rend(); ++iterator){
				const FrameSpikeRecord& spike = *iterator;
				ImGui::PushID(static_cast<int>(spike.frame & 0x7fffffff));
				char header[192]{};
				std::snprintf(
					header,
					sizeof(header),
					"Frame %llu · %.2f ms · %s%s%s",
					static_cast<unsigned long long>(spike.frame),
					spike.peakMilliseconds,
					spike.dominantSection.c_str(),
					spike.startup ? " · Startup" : "",
					spike.resize ? " · Resize" : ""
				);
				if(ImGui::TreeNodeEx(header, ImGuiTreeNodeFlags_SpanAvailWidth)){
					ImGui::Text("Update %.3f ms", spike.updateMilliseconds);
					ImGui::Text("Draw %.3f ms", spike.drawMilliseconds);
					ImGui::Text(
						"GPU %.3f ms (%s)",
						spike.gpuMilliseconds,
						GpuTimingStatusName(spike.gpuStatus)
					);
					ImGui::Text("Frame pacing %.3f ms", spike.framePacingMilliseconds);
					ImGui::Text("Render schedule %.3f ms", spike.renderMilliseconds);
					ImGui::Text("Editor UI %.3f ms", spike.editorMilliseconds);
					ImGui::Text("Present %.3f ms", spike.presentMilliseconds);
					ImGui::Text("Unaccounted %.3f ms", spike.unaccountedMilliseconds);
					if(!spike.dominantPanel.empty()){
						ImGui::TextDisabled(
							"Editor peak · %s %.3f ms",
							spike.dominantPanel.c_str(),
							spike.dominantPanelMilliseconds
						);
					}
					if(spike.resize){
						ImGui::TextDisabled(
							"Resize CPU %.3f ms",
							spike.resizeMilliseconds
						);
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
		}
		ImGui::Unindent(4.0f);
		ImGui::Spacing();
	}

	if(SectionHeader("MemoryHistory", "Memory", false)){
		ImGui::Indent(4.0f);
		const float usageCurrent = UsageSamples[SAMPLE_LENGTH - 1];
		BudgetPlot(
			"Process Usage",
			"UsageHistory",
			UsageSamples,
			SAMPLE_LENGTH,
			usageCurrent,
			Average(UsageSamples, SAMPLE_LENGTH, true),
			100.0f
		);
		BudgetPlot(
			"Working Set",
			"WorkingSetHistory",
			WorkingSetSizeSamples,
			SAMPLE_LENGTH,
			workingSetMb,
			Average(WorkingSetSizeSamples, SAMPLE_LENGTH, true),
			0.0f
		);
		BudgetPlot(
			"Commit",
			"CommitHistory",
			CommitSizeSamples,
			SAMPLE_LENGTH,
			memoryAvailable
				? static_cast<float>(memory.PagefileUsage / 1000000.0)
				: 0.0f,
			Average(CommitSizeSamples, SAMPLE_LENGTH, true),
			0.0f
		);
		ImGui::Unindent(4.0f);
		ImGui::Spacing();
	}

	if(SectionHeader("Debug", "Debug", false)){
		ImGui::Indent(4.0f);
		GraphicsContext* graphics = m_editor->sceneManager
			? m_editor->sceneManager->GetContext()->graphics
			: nullptr;
		ImGui::BeginDisabled(!graphics || graphics->IsDeviceLost());
		if(ImGui::Button("Simulate Device Lost") && graphics){
			graphics->MarkDeviceLostForTest();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled(
			"Closes the app through the graceful device-lost path."
		);
		if(graphics && graphics->IsDeviceLost()){
			ImGui::TextUnformatted("Device Lost: transitioning to shutdown.");
		}
		ImGui::Unindent(4.0f);
	}

	ImGui::End();
}
