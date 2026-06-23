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

void PerformanceMonitor::RecordFrameSpike(const EditorDrawContext& ctx){
	++FrameSequence;

	if(ctx.ResizeSerial != LastResizeSerial){
		LastResizeSerial = ctx.ResizeSerial;
		ResizeGraceFrames = 8;
	}

	const float updateMs = static_cast<float>(ctx.UpdateTime * 1000.0);
	const float drawMs = static_cast<float>(ctx.DrawTime * 1000.0);
	const float gpuMs = ctx.GPUFrameTimeValid
		? static_cast<float>(ctx.GPUFrameTime * 1000.0)
		: 0.0f;
	const float renderMs = static_cast<float>(ctx.DrawTiming.renderSchedule * 1000.0);
	const float editorMs = static_cast<float>(ctx.DrawTiming.editorUIBuild * 1000.0);
	const float presentMs = static_cast<float>(ctx.DrawTiming.present * 1000.0);
	const float peakMs = (std::max)(updateMs, (std::max)(drawMs, gpuMs));
	const bool spike = peakMs >= SpikeThresholdMilliseconds;

	if(!spike){
		InSpikeSequence = false;
		if(ResizeGraceFrames > 0) --ResizeGraceFrames;
		return;
	}

	FrameSpikeRecord record{};
	record.frame = FrameSequence;
	record.peakMilliseconds = peakMs;
	record.updateMilliseconds = updateMs;
	record.drawMilliseconds = drawMs;
	record.gpuMilliseconds = gpuMs;
	record.renderMilliseconds = renderMs;
	record.editorMilliseconds = editorMs;
	record.presentMilliseconds = presentMs;
	record.resizeMilliseconds = static_cast<float>(ctx.LastResizeCpuTime * 1000.0);
	record.startup = FrameSequence <= 180;
	record.resize = ResizeGraceFrames > 0;

	auto considerDominant = [&](const char* name, float milliseconds){
		if(milliseconds > record.dominantMilliseconds){
			record.dominantMilliseconds = milliseconds;
			record.dominantSection = name;
		}
	};
	considerDominant("Update CPU", updateMs);
	considerDominant("Render Schedule CPU", renderMs);
	considerDominant("Editor UI CPU", editorMs);
	considerDominant("Present / Queue Wait", presentMs);
	considerDominant("GPU Frame", gpuMs);

	if(ctx.EditorPanelTimings){
		for(const EditorPanelTiming& timing : *ctx.EditorPanelTimings){
			const float milliseconds = static_cast<float>(timing.seconds * 1000.0);
			if(milliseconds > record.dominantPanelMilliseconds){
				record.dominantPanelMilliseconds = milliseconds;
				record.dominantPanel = timing.name ? timing.name : "Unknown";
			}
		}
	}

	if(InSpikeSequence && !FrameSpikes.empty()){
		FrameSpikeRecord& active = FrameSpikes.back();
		active.startup = active.startup || record.startup;
		active.resize = active.resize || record.resize;
		active.resizeMilliseconds = (std::max)(active.resizeMilliseconds, record.resizeMilliseconds);
		if(record.peakMilliseconds > active.peakMilliseconds){
			const bool startup = active.startup;
			const bool resize = active.resize;
			const float resizeMs = active.resizeMilliseconds;
			active = std::move(record);
			active.startup = startup;
			active.resize = resize;
			active.resizeMilliseconds = resizeMs;
		}
	} else{
		FrameSpikes.push_back(std::move(record));
		if(FrameSpikes.size() > 32) FrameSpikes.pop_front();
	}
	InSpikeSequence = true;
	if(ResizeGraceFrames > 0) --ResizeGraceFrames;
}

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

	RecordFrameSpike(ctx);

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

	if(ImGui::TreeNodeEx("Frame Spike Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)){
		ImGui::SetNextItemWidth(140.0f);
		ImGui::SliderFloat("Threshold (ms)", &SpikeThresholdMilliseconds, 5.0f, 100.0f, "%.1f");
		ImGui::SameLine();
		if(ImGui::Button("Clear Spikes")){
			FrameSpikes.clear();
			InSpikeSequence = false;
		}
		ImGui::TextDisabled("One record per contiguous spike sequence; latest 32 sequences");

		if(FrameSpikes.empty()){
			ImGui::TextDisabled("No frame spikes above threshold");
		} else{
			for(auto it = FrameSpikes.rbegin(); it != FrameSpikes.rend(); ++it){
				const FrameSpikeRecord& spike = *it;
				ImGui::PushID(static_cast<int>(spike.frame));
				ImGui::BulletText(
					"Frame %llu Peak %.3fms / %s %.3fms%s%s",
					static_cast<unsigned long long>(spike.frame),
					spike.peakMilliseconds,
					spike.dominantSection.c_str(),
					spike.dominantMilliseconds,
					spike.startup ? " [Startup]" : "",
					spike.resize ? " [Resize]" : ""
				);
				ImGui::TextDisabled(
					"Update %.3f / Draw %.3f / GPU %.3f / Render %.3f / Editor %.3f / Present %.3f ms",
					spike.updateMilliseconds,
					spike.drawMilliseconds,
					spike.gpuMilliseconds,
					spike.renderMilliseconds,
					spike.editorMilliseconds,
					spike.presentMilliseconds
				);
				if(!spike.dominantPanel.empty()){
					ImGui::TextDisabled(
						"Editor peak: %s %.3fms",
						spike.dominantPanel.c_str(),
						spike.dominantPanelMilliseconds
					);
				}
				if(spike.resize){
					ImGui::TextDisabled("Resize CPU: %.3fms", spike.resizeMilliseconds);
				}
				ImGui::PopID();
			}
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
