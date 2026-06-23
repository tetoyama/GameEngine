from pathlib import Path


def load(path: str) -> str:
    return Path(path).read_text(encoding="utf-8-sig")


def save(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


# AssetsBrowser explicitly owns shared_ptr fields.
assets_h = "Source/GameApplication/Engine/Editor/UI/AssetsBrowser.h"
text = load(assets_h)
text = replace_once(text, "#include <filesystem>\n", "#include <filesystem>\n#include <memory>\n", "assets memory include")
save(assets_h, text)

# Pass resize diagnostics from MainRenderer to the editor snapshot.
engine_cpp = "Source/GameApplication/Engine/engine.cpp"
text = load(engine_cpp)
text = replace_once(
    text,
    """\t\t\tdraw.VSyncEnabled = config->appConfig.Vsync;
\t\t\tdraw.TearingSupported = graphics->IsTearingSupported();
""",
    """\t\t\tdraw.VSyncEnabled = config->appConfig.Vsync;
\t\t\tdraw.TearingSupported = graphics->IsTearingSupported();
\t\t\tdraw.ResizeSerial = renderer->GetResizeSerial();
\t\t\tdraw.LastResizeCpuTime = renderer->GetLastResizeCpuTimeSeconds();
""",
    "engine resize snapshot",
)
save(engine_cpp, text)

# Add a small grace period so GPU warm-up frames after Resize are tagged too.
monitor_h = "Source/GameApplication/Engine/Editor/UI/PerformanceMonitor.h"
text = load(monitor_h)
text = replace_once(
    text,
    """\tfloat SpikeThresholdMilliseconds = 20.0f;
\tbool InSpikeSequence = false;
""",
    """\tfloat SpikeThresholdMilliseconds = 20.0f;
\tbool InSpikeSequence = false;
\tint ResizeGraceFrames = 0;
""",
    "resize grace member",
)
save(monitor_h, text)

monitor_cpp = "Source/GameApplication/Engine/Editor/UI/PerformanceMonitor.cpp"
text = load(monitor_cpp)
method_marker = "void PerformanceMonitor::Draw(const EditorDrawContext ctx)"
method = r'''void PerformanceMonitor::RecordFrameSpike(const EditorDrawContext& ctx){
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
	const float peakMs = std::max(updateMs, std::max(drawMs, gpuMs));
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
		active.resizeMilliseconds = std::max(active.resizeMilliseconds, record.resizeMilliseconds);
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

'''
text = replace_once(text, method_marker, method + method_marker, "spike method")
text = replace_once(
    text,
    """\tif(!showPerformanceMonitor || !*showPerformanceMonitor) {
""",
    """\tRecordFrameSpike(ctx);

\tif(!showPerformanceMonitor || !*showPerformanceMonitor) {
""",
    "spike record call",
)
spike_ui = r'''
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

'''
text = replace_once(
    text,
    '\tif (ImGui::TreeNodeEx("-メモリ使用量-", ImGuiTreeNodeFlags_DefaultOpen)) {',
    spike_ui + '\tif (ImGui::TreeNodeEx("-メモリ使用量-", ImGuiTreeNodeFlags_DefaultOpen)) {',
    "spike diagnostics UI",
)
save(monitor_cpp, text)
