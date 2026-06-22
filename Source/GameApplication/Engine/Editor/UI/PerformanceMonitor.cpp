// =======================================================================
// 
// PerformanceMonitor.cpp
// 
// =======================================================================
#include "PerformanceMonitor.h"
#include <Psapi.h>
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"

void PerformanceMonitor::Draw(const EditorDrawContext ctx) {

	double FPS = ctx.FPS;
	double FixedFPS = ctx.FixedUpdateFPS;
	double Draw = ctx.DrawTime;
	double Update = ctx.UpdateTime;
	bool* showPerformanceMonitor = &m_editor->GetUI<MenuBar>()->showPerformanceMonitor;

	HANDLE hProc = GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS_EX pmc{};
	BOOL isSuccess = GetProcessMemoryInfo(
		hProc,
		(PROCESS_MEMORY_COUNTERS*)&pmc,
		sizeof(pmc));
	if (isSuccess == FALSE) {
		return;
	}

	auto shiftSamples = [](float* samples) {
		for (int n = 0; n < SAMPLE_LENGTH - 1; n++) {
			samples[n] = samples[n + 1];
		}
	};

	auto pushMilliseconds = [&](float* samples, double seconds) {
		shiftSamples(samples);
		samples[SAMPLE_LENGTH - 1] = static_cast<float>(seconds * 1000.0);
	};

	SampleCount = (SampleCount + 1) % TARGET_FPS;
	if (SampleCount == 0) {
		shiftSamples(FixedFpsSamples);
		shiftSamples(DeltaFpsSamples);
		shiftSamples(UsageSamples);
		shiftSamples(CommitSizeSamples);
		shiftSamples(WorkingSetSizeSamples);

		FixedFpsSamples[SAMPLE_LENGTH - 1] = static_cast<float>(FixedFPS);
		DeltaFpsSamples[SAMPLE_LENGTH - 1] = static_cast<float>(FPS);

		const SIZE_T usageBase = pmc.WorkingSetSize + pmc.PagefileUsage;
		UsageSamples[SAMPLE_LENGTH - 1] = usageBase > 0
			? 100.0f * static_cast<float>(pmc.WorkingSetSize) / static_cast<float>(usageBase)
			: 0.0f;
		CommitSizeSamples[SAMPLE_LENGTH - 1] = pmc.PagefileUsage / 1000000.0f;
		WorkingSetSizeSamples[SAMPLE_LENGTH - 1] = pmc.WorkingSetSize / 1000000.0f;
	}

	pushMilliseconds(UpdateSamples, Update);
	pushMilliseconds(DrawSamples, Draw);
	pushMilliseconds(FrameSetupSamples, ctx.DrawTiming.frameSetup);
	pushMilliseconds(ImGuiBeginSamples, ctx.DrawTiming.imguiBegin);
	pushMilliseconds(RenderScheduleSamples, ctx.DrawTiming.renderSchedule);
	pushMilliseconds(DebugDrawSamples, ctx.DrawTiming.debugDraw);
	pushMilliseconds(EditorUIBuildSamples, ctx.DrawTiming.editorUIBuild);
	pushMilliseconds(ImGuiRenderSamples, ctx.DrawTiming.imguiRender);
	pushMilliseconds(PresentSamples, ctx.DrawTiming.present);
	pushMilliseconds(UnaccountedSamples, ctx.DrawTiming.GetUnaccountedTime());
	pushMilliseconds(
		GPUFrameTimeSamples,
		ctx.GPUFrameTimeValid ? ctx.GPUFrameTime : 0.0
	);

	// 全Panelの系列を1フレーム進め、前回完了した計測値を末尾へ設定する。
	for(auto& series : PanelTimingSamples){
		shiftSamples(series.samples.data());
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

	if(!showPerformanceMonitor || !*showPerformanceMonitor) {
		return;
	}

	ImGuiWindowClass window_class;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);

	ImGuiWindowFlags toolbar_window_flags = 0;
	ImGui::Begin("Performance Monitor", showPerformanceMonitor, toolbar_window_flags);

	auto averageSamples = [](const float* samples) {
		float total = 0.0f;
		for (int n = 0; n < SAMPLE_LENGTH; n++) {
			total += samples[n];
		}
		return total / static_cast<float>(SAMPLE_LENGTH);
	};

	auto averageValidSamples = [](const float* samples) {
		float total = 0.0f;
		int count = 0;
		for (int n = 0; n < SAMPLE_LENGTH; n++) {
			if(samples[n] > 0.0f){
				total += samples[n];
				count++;
			}
		}
		return count > 0 ? total / static_cast<float>(count) : 0.0f;
	};

	if (ImGui::TreeNodeEx("負荷計測", ImGuiTreeNodeFlags_DefaultOpen)) {
		float FixedFPSAvg = 0.0f;
		float DeltaFPSAvg = 0.0f;
		int FPSCount = 0;
		for (int n = 0; n < SAMPLE_LENGTH; n++) {
			if (0 < FixedFpsSamples[n]) {
				FixedFPSAvg += FixedFpsSamples[n];
				DeltaFPSAvg += DeltaFpsSamples[n];
				FPSCount++;
			}
		}
		if (0 < FPSCount) {
			FixedFPSAvg /= FPSCount;
			DeltaFPSAvg /= FPSCount;
		}

		char Texts[128]{};
		ImGui::Text("-FPS計測-");
		std::snprintf(Texts, sizeof(Texts), "Fixed:%.2f Avg:%.2f", FixedFpsSamples[SAMPLE_LENGTH - 1], FixedFPSAvg);
		ImGui::Text("%s", Texts);
		ImGui::PlotLines("##Fixed", FixedFpsSamples, SAMPLE_LENGTH, 0, "", 0.0f);
		std::snprintf(Texts, sizeof(Texts), "Delta:%.2f Avg:%.2f", DeltaFpsSamples[SAMPLE_LENGTH - 1], DeltaFPSAvg);
		ImGui::Text("%s", Texts);
		ImGui::PlotLines("##Delta", DeltaFpsSamples, SAMPLE_LENGTH, 0, "", 0.0f);

		ImGui::Text("-更新処理-");
		std::snprintf(Texts, sizeof(Texts), "Update: Current %.4fms Avg %.4fms", UpdateSamples[SAMPLE_LENGTH - 1], averageSamples(UpdateSamples));
		ImGui::Text("%s", Texts);
		ImGui::PlotLines("##Update", UpdateSamples, SAMPLE_LENGTH, 0, "", 0.0f, 1000.0f / 60.0f);

		ImGui::Text("-描画処理-");
		std::snprintf(Texts, sizeof(Texts), "Draw: Current %.4fms Avg %.4fms", DrawSamples[SAMPLE_LENGTH - 1], averageSamples(DrawSamples));
		ImGui::Text("%s", Texts);
		ImGui::PlotLines("##Draw", DrawSamples, SAMPLE_LENGTH, 0, "", 0.0f, 1000.0f / 60.0f);
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("描画CPU / GPU内訳", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("VSync: %s", ctx.VSyncEnabled ? "ON" : "OFF");
		ImGui::Text("Tearing: %s", ctx.TearingSupported ? "Supported" : "Unsupported");
		if(ctx.GPUFrameTimeValid){
			ImGui::Text(
				"GPU Frame Time: Current %.4fms Avg %.4fms",
				GPUFrameTimeSamples[SAMPLE_LENGTH - 1],
				averageValidSamples(GPUFrameTimeSamples)
			);
			ImGui::PlotLines(
				"##GPUFrameTime",
				GPUFrameTimeSamples,
				SAMPLE_LENGTH,
				0,
				"",
				0.0f,
				1000.0f / 60.0f
			);
		} else{
			ImGui::TextDisabled("GPU Frame Time: waiting for asynchronous query result");
		}
		ImGui::Separator();

		auto drawTimingRow = [&](const char* label, const char* plotId, float* samples) {
			ImGui::Text(
				"%s: Current %.4fms Avg %.4fms",
				label,
				samples[SAMPLE_LENGTH - 1],
				averageSamples(samples)
			);
			ImGui::PlotLines(
				plotId,
				samples,
				SAMPLE_LENGTH,
				0,
				"",
				0.0f,
				1000.0f / 60.0f
			);
		};

		drawTimingRow("Frame Setup", "##DrawFrameSetup", FrameSetupSamples);
		drawTimingRow("ImGui Begin / UI Frame", "##DrawImGuiBegin", ImGuiBeginSamples);
		drawTimingRow("Render Schedule CPU", "##DrawRenderSchedule", RenderScheduleSamples);
		drawTimingRow("Debug Draw CPU", "##DrawDebug", DebugDrawSamples);
		drawTimingRow("Editor UI Build CPU", "##DrawEditorUI", EditorUIBuildSamples);
		drawTimingRow("ImGui Render / Platform Windows", "##DrawImGuiRender", ImGuiRenderSamples);
		drawTimingRow("Present / Queue Wait", "##DrawPresent", PresentSamples);
		drawTimingRow("Unaccounted / Timer Overhead", "##DrawUnaccounted", UnaccountedSamples);

		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Editor Panel CPU", ImGuiTreeNodeFlags_DefaultOpen)) {
		for(auto& series : PanelTimingSamples){
			ImGui::PushID(series.name.c_str());
			ImGui::Text(
				"%s: Current %.4fms Avg %.4fms",
				series.name.c_str(),
				series.samples[SAMPLE_LENGTH - 1],
				averageSamples(series.samples.data())
			);
			ImGui::PlotLines(
				"##PanelTiming",
				series.samples.data(),
				SAMPLE_LENGTH,
				0,
				"",
				0.0f,
				1000.0f / 60.0f
			);
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("-メモリ使用量-", ImGuiTreeNodeFlags_DefaultOpen)) {
		float UsageAvg = 0.0f;
		int FPSCount = 0;
		for (int n = 0; n < SAMPLE_LENGTH; n++) {
			if (0 < FixedFpsSamples[n]) {
				UsageAvg += UsageSamples[n];
				FPSCount++;
			}
		}
		if (0 < FPSCount) {
			UsageAvg /= FPSCount;
		}
		char Texts[64]{};
		std::snprintf(Texts, sizeof(Texts), "usage:Avg:%.2f%%", UsageAvg);
		ImGui::PlotLines(Texts, UsageSamples, SAMPLE_LENGTH, 0, "", 0.0f, 100.0f);
		std::snprintf(Texts, sizeof(Texts), "Commit:%dMB", (int)(pmc.PagefileUsage / 1000000));
		ImGui::PlotLines(Texts, CommitSizeSamples, SAMPLE_LENGTH, 0, "", 0.0f, pmc.PeakPagefileUsage / 1000000.0f);
		std::snprintf(Texts, sizeof(Texts), "Working:%dMB", (int)(pmc.WorkingSetSize / 1000000));
		ImGui::PlotLines(Texts, WorkingSetSizeSamples, SAMPLE_LENGTH, 0, "", 0.0f, pmc.PeakWorkingSetSize / 1000000.0f);
		ImGui::TreePop();
	}
	ImGui::End();
}
