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

namespace {

template<typename T, size_t N>
void ShiftSamples(T (&samples)[N]){
	for(size_t index = 0; index + 1 < N; ++index){
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

	for(size_t index = 0; index < SAMPLE_LENGTH; ++index){
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
		} else{
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
	RecordCompletedFrame(ctx);
	ApplyGpuFrameTimings(ctx);
	RebuildFrameSpikes();

	const double FPS = ctx.FPS;
	const double FixedFPS = ctx.FixedUpdateFPS;
	bool* showPerformanceMonitor = &m_editor->GetUI<MenuBar>()->showPerformanceMonitor;

	PROCESS_MEMORY_COUNTERS_EX pmc{};
	const BOOL memoryAvailable = GetProcessMemoryInfo(
		GetCurrentProcess(),
		reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
		sizeof(pmc)
	);

	SampleCount = (SampleCount + 1) % TARGET_FPS;
	if(SampleCount == 0){
		ShiftSamples(FixedFpsSamples);
		ShiftSamples(DeltaFpsSamples);
		ShiftSamples(UsageSamples);
		ShiftSamples(CommitSizeSamples);
		ShiftSamples(WorkingSetSizeSamples);
		FixedFpsSamples[SAMPLE_LENGTH - 1] = static_cast<float>(FixedFPS);
		DeltaFpsSamples[SAMPLE_LENGTH - 1] = static_cast<float>(FPS);

		if(memoryAvailable){
			const SIZE_T usageBase = pmc.WorkingSetSize + pmc.PagefileUsage;
			UsageSamples[SAMPLE_LENGTH - 1] = usageBase > 0
				? 100.0f * static_cast<float>(pmc.WorkingSetSize) /
					static_cast<float>(usageBase)
				: 0.0f;
			CommitSizeSamples[SAMPLE_LENGTH - 1] =
				pmc.PagefileUsage / 1000000.0f;
			WorkingSetSizeSamples[SAMPLE_LENGTH - 1] =
				pmc.WorkingSetSize / 1000000.0f;
		}
	}

	if(!showPerformanceMonitor || !*showPerformanceMonitor){
		return;
	}

	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&windowClass);
	ImGui::Begin("Performance Monitor", showPerformanceMonitor);

	auto averageSamples = [](const float* samples){
		float total = 0.0f;
		for(int index = 0; index < SAMPLE_LENGTH; ++index){
			total += samples[index];
		}
		return total / static_cast<float>(SAMPLE_LENGTH);
	};
	auto averageValidSamples = [](const float* samples){
		float total = 0.0f;
		int count = 0;
		for(int index = 0; index < SAMPLE_LENGTH; ++index){
			if(samples[index] > 0.0f){
				total += samples[index];
				++count;
			}
		}
		return count > 0 ? total / static_cast<float>(count) : 0.0f;
	};

	if(ImGui::TreeNodeEx("負荷計測", ImGuiTreeNodeFlags_DefaultOpen)){
		float fixedAverage = 0.0f;
		float frameAverage = 0.0f;
		int count = 0;
		for(int index = 0; index < SAMPLE_LENGTH; ++index){
			if(FixedFpsSamples[index] > 0.0f){
				fixedAverage += FixedFpsSamples[index];
				frameAverage += DeltaFpsSamples[index];
				++count;
			}
		}
		if(count > 0){
			fixedAverage /= count;
			frameAverage /= count;
		}

		ImGui::Text("Fixed: %.2f Avg: %.2f",
			FixedFpsSamples[SAMPLE_LENGTH - 1], fixedAverage);
		ImGui::PlotLines("##Fixed", FixedFpsSamples, SAMPLE_LENGTH);
		ImGui::Text("Frame: %.2f Avg: %.2f",
			DeltaFpsSamples[SAMPLE_LENGTH - 1], frameAverage);
		ImGui::PlotLines("##Frame", DeltaFpsSamples, SAMPLE_LENGTH);
		ImGui::Text("Update: Current %.4fms Avg %.4fms",
			UpdateSamples[SAMPLE_LENGTH - 1], averageSamples(UpdateSamples));
		ImGui::PlotLines("##Update", UpdateSamples, SAMPLE_LENGTH, 0, "", 0.0f, 1000.0f / 60.0f);
		ImGui::Text("Draw: Current %.4fms Avg %.4fms",
			DrawSamples[SAMPLE_LENGTH - 1], averageSamples(DrawSamples));
		ImGui::PlotLines("##Draw", DrawSamples, SAMPLE_LENGTH, 0, "", 0.0f, 1000.0f / 60.0f);
		ImGui::TreePop();
	}

	if(ImGui::TreeNodeEx("描画CPU / GPU内訳", ImGuiTreeNodeFlags_DefaultOpen)){
		ImGui::Text("VSync: %s", ctx.VSyncEnabled ? "ON" : "OFF");
		ImGui::Text("Tearing: %s", ctx.TearingSupported ? "Supported" : "Unsupported");
		ImGui::Text(
			"Frame Pacing: %s / Timeouts %llu",
			ctx.FrameLatencyWaitableObjectEnabled ? "Waitable Object" : "DXGI Fallback",
			static_cast<unsigned long long>(ctx.FrameLatencyWaitTimeoutCount)
		);

		const FrameTimingRecord* latest =
			FrameHistory.empty() ? nullptr : &FrameHistory.back();
		const FrameTimingRecord* latestResolved = nullptr;
		for(auto it = FrameHistory.rbegin(); it != FrameHistory.rend(); ++it){
			if(it->gpuStatus == GpuFrameTimingStatus::Resolved){
				latestResolved = &*it;
				break;
			}
		}

		if(latest){
			ImGui::Text(
				"GPU status: Frame %llu / %s",
				static_cast<unsigned long long>(latest->frame),
				GpuTimingStatusName(latest->gpuStatus)
			);
		}
		if(latestResolved){
			ImGui::Text(
				"GPU latest resolved: Frame %llu %.4fms / Avg %.4fms",
				static_cast<unsigned long long>(latestResolved->frame),
				latestResolved->gpuMilliseconds,
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
			ImGui::TextDisabled("GPU Frame Time: waiting for a resolved query result");
		}
		ImGui::Separator();

		auto drawTimingRow = [&](const char* label, const char* plotId, float* samples){
			ImGui::Text(
				"%s: Current %.4fms Avg %.4fms",
				label,
				samples[SAMPLE_LENGTH - 1],
				averageSamples(samples)
			);
			ImGui::PlotLines(plotId, samples, SAMPLE_LENGTH, 0, "", 0.0f, 1000.0f / 60.0f);
		};
		drawTimingRow("Frame Pacing Wait", "##FramePacingWait", FramePacingWaitSamples);
		drawTimingRow("Frame Setup", "##DrawFrameSetup", FrameSetupSamples);
		drawTimingRow("ImGui Begin / UI Frame", "##DrawImGuiBegin", ImGuiBeginSamples);
		drawTimingRow("Render Schedule CPU", "##DrawRenderSchedule", RenderScheduleSamples);
		drawTimingRow("Debug Draw CPU", "##DrawDebug", DebugDrawSamples);
		drawTimingRow("Editor UI Build CPU", "##DrawEditorUI", EditorUIBuildSamples);
		drawTimingRow("ImGui Render / Platform Windows", "##DrawImGuiRender", ImGuiRenderSamples);
		drawTimingRow("Present / Residual Queue Wait", "##DrawPresent", PresentSamples);
		drawTimingRow("Unaccounted Draw CPU", "##DrawUnaccounted", UnaccountedSamples);
		ImGui::TreePop();
	}

	if(ImGui::TreeNodeEx("Editor Panel CPU", ImGuiTreeNodeFlags_DefaultOpen)){
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
			FrameHistory.clear();
			FrameSpikes.clear();
			DeferredGpuResults.clear();
		}
		ImGui::TextDisabled("CPU/GPU values are joined only when their Frame Serial matches");

		if(FrameSpikes.empty()){
			ImGui::TextDisabled("No frame spikes above threshold");
		} else{
			for(auto it = FrameSpikes.rbegin(); it != FrameSpikes.rend(); ++it){
				const FrameSpikeRecord& spike = *it;
				ImGui::PushID(static_cast<int>(spike.frame & 0x7fffffff));
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
					"Update %.3f / Draw %.3f / GPU %.3f (%s) / Pacing %.3f / Render %.3f / Editor %.3f / Present %.3f / Unaccounted %.3f ms",
					spike.updateMilliseconds,
					spike.drawMilliseconds,
					spike.gpuMilliseconds,
					GpuTimingStatusName(spike.gpuStatus),
					spike.framePacingMilliseconds,
					spike.renderMilliseconds,
					spike.editorMilliseconds,
					spike.presentMilliseconds,
					spike.unaccountedMilliseconds
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

	if(ImGui::TreeNodeEx("-メモリ使用量-", ImGuiTreeNodeFlags_DefaultOpen)){
		float usageAverage = 0.0f;
		int count = 0;
		for(int index = 0; index < SAMPLE_LENGTH; ++index){
			if(FixedFpsSamples[index] > 0.0f){
				usageAverage += UsageSamples[index];
				++count;
			}
		}
		if(count > 0){
			usageAverage /= count;
		}
		char text[64]{};
		std::snprintf(text, sizeof(text), "usage:Avg:%.2f%%", usageAverage);
		ImGui::PlotLines(text, UsageSamples, SAMPLE_LENGTH, 0, "", 0.0f, 100.0f);
		std::snprintf(text, sizeof(text), "Commit:%dMB", static_cast<int>(pmc.PagefileUsage / 1000000));
		ImGui::PlotLines(text, CommitSizeSamples, SAMPLE_LENGTH);
		std::snprintf(text, sizeof(text), "Working:%dMB", static_cast<int>(pmc.WorkingSetSize / 1000000));
		ImGui::PlotLines(text, WorkingSetSizeSamples, SAMPLE_LENGTH);
		ImGui::TreePop();
	}

	ImGui::End();
}
