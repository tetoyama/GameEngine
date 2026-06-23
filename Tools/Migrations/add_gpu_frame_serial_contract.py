from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8-sig")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


# ---------------------------------------------------------------------------
# Shared GPU timing result contract.
# ---------------------------------------------------------------------------
write(
    "Source/GameApplication/Service/Graphics/GpuFrameTiming.h",
    r'''#pragma once

#include <cstdint>

enum class GpuFrameTimingStatus : std::uint8_t {
	Pending = 0,
	Resolved,
	Invalid,
	Dropped
};

struct GpuFrameTimingResult {
	std::uint64_t frameSerial = 0;
	double seconds = 0.0;
	GpuFrameTimingStatus status = GpuFrameTimingStatus::Invalid;
};
'''
)


# ---------------------------------------------------------------------------
# GraphicsContext: serial-tagged queries and one-shot result consumption.
# ---------------------------------------------------------------------------
graphics_h = "Source/GameApplication/Service/Graphics/graphicsContext.h"
text = read(graphics_h)
text = replace_once(
    text,
    '#include "Service/IService.h"\n',
    '#include "Service/IService.h"\n#include "GpuFrameTiming.h"\n',
    "GraphicsContext timing include",
)
text = replace_once(
    text,
    '\t// GPU Timestamp Queryは複数フレーム遅延で回収し、CPUを待機させない。\n'
    '\tvoid BeginGpuFrameTiming();\n'
    '\tvoid EndGpuFrameTiming();\n'
    '\tbool HasValidGpuFrameTime() const noexcept { return m_GpuFrameTimeValid; }\n'
    '\tdouble GetGpuFrameTimeSeconds() const noexcept { return m_GpuFrameTimeSeconds; }\n',
    '\t// GPU Timestamp QueryはFrame Serial付きで非同期回収する。\n'
    '\t// Consumeは未消費結果だけを提出順に一度だけ返す。\n'
    '\tvoid BeginGpuFrameTiming(uint64_t frameSerial);\n'
    '\tvoid EndGpuFrameTiming();\n'
    '\tstd::vector<GpuFrameTimingResult> ConsumeResolvedGpuFrameTimings();\n',
    "GraphicsContext public timing API",
)
text = replace_once(
    text,
    '\tstruct GpuFrameTimingQuerySet {\n'
    '\t\tMicrosoft::WRL::ComPtr<ID3D11Query> disjoint;\n'
    '\t\tMicrosoft::WRL::ComPtr<ID3D11Query> beginTimestamp;\n'
    '\t\tMicrosoft::WRL::ComPtr<ID3D11Query> endTimestamp;\n'
    '\t\tbool pending = false;\n'
    '\t};',
    '\tstruct GpuFrameTimingQuerySet {\n'
    '\t\tMicrosoft::WRL::ComPtr<ID3D11Query> disjoint;\n'
    '\t\tMicrosoft::WRL::ComPtr<ID3D11Query> beginTimestamp;\n'
    '\t\tMicrosoft::WRL::ComPtr<ID3D11Query> endTimestamp;\n'
    '\t\tuint64_t submittedFrameSerial = 0;\n'
    '\t\tbool pending = false;\n'
    '\t};',
    "GPU query serial",
)
text = replace_once(
    text,
    '\tbool m_GpuTimingAvailable = false;\n'
    '\tbool m_GpuFrameTimeValid = false;\n'
    '\tdouble m_GpuFrameTimeSeconds = 0.0;\n',
    '\tbool m_GpuTimingAvailable = false;\n'
    '\tstd::vector<GpuFrameTimingResult> m_ResolvedGpuFrameTimings;\n',
    "GPU result storage",
)
write(graphics_h, text)


graphics_cpp = "Source/GameApplication/Service/Graphics/graphicsContext.cpp"
text = read(graphics_cpp)
text = replace_once(
    text,
    '#include <stdexcept>\n',
    '#include <algorithm>\n#include <stdexcept>\n',
    "GraphicsContext algorithm include",
)
pattern = re.compile(
    r'bool GraphicsContext::CreateGpuFrameTimingQueries\(\)\{.*?'
    r'void GraphicsContext::EndGpuFrameTiming\(\)\{.*?\n\}\n\n',
    re.S,
)
replacement = r'''bool GraphicsContext::CreateGpuFrameTimingQueries(){
	if(!m_Device || GetBackendType() != RHI::BackendType::Direct3D11){
		return false;
	}

	D3D11_QUERY_DESC desc{};
	for(auto& timing : m_GpuTimingQueries){
		desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		if(FAILED(m_Device->CreateQuery(&desc, timing.disjoint.ReleaseAndGetAddressOf()))){
			return false;
		}

		desc.Query = D3D11_QUERY_TIMESTAMP;
		if(FAILED(m_Device->CreateQuery(&desc, timing.beginTimestamp.ReleaseAndGetAddressOf()))){
			return false;
		}
		if(FAILED(m_Device->CreateQuery(&desc, timing.endTimestamp.ReleaseAndGetAddressOf()))){
			return false;
		}
		timing.submittedFrameSerial = 0;
		timing.pending = false;
	}

	m_GpuTimingWriteIndex = 0;
	m_ActiveGpuTimingIndex = -1;
	m_ResolvedGpuFrameTimings.clear();
	m_GpuTimingAvailable = true;
	return true;
}

void GraphicsContext::ResolveGpuFrameTimingQueries(){
	if(!m_GpuTimingAvailable || !m_DeviceContext){
		return;
	}

	for(auto& timing : m_GpuTimingQueries){
		if(!timing.pending){
			continue;
		}

		auto complete = [&](GpuFrameTimingStatus status, double seconds = 0.0){
			m_ResolvedGpuFrameTimings.push_back({
				timing.submittedFrameSerial,
				seconds,
				status
			});
			timing.pending = false;
			timing.submittedFrameSerial = 0;
		};

		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
		const HRESULT disjointResult = m_DeviceContext->GetData(
			timing.disjoint.Get(),
			&disjoint,
			sizeof(disjoint),
			D3D11_ASYNC_GETDATA_DONOTFLUSH
		);
		if(disjointResult == S_FALSE){
			continue;
		}
		if(FAILED(disjointResult)){
			complete(GpuFrameTimingStatus::Invalid);
			continue;
		}

		UINT64 beginTimestamp = 0;
		const HRESULT beginResult = m_DeviceContext->GetData(
			timing.beginTimestamp.Get(),
			&beginTimestamp,
			sizeof(beginTimestamp),
			D3D11_ASYNC_GETDATA_DONOTFLUSH
		);
		if(beginResult == S_FALSE){
			continue;
		}
		if(FAILED(beginResult)){
			complete(GpuFrameTimingStatus::Invalid);
			continue;
		}

		UINT64 endTimestamp = 0;
		const HRESULT endResult = m_DeviceContext->GetData(
			timing.endTimestamp.Get(),
			&endTimestamp,
			sizeof(endTimestamp),
			D3D11_ASYNC_GETDATA_DONOTFLUSH
		);
		if(endResult == S_FALSE){
			continue;
		}
		if(FAILED(endResult)){
			complete(GpuFrameTimingStatus::Invalid);
			continue;
		}

		if(disjoint.Disjoint || disjoint.Frequency == 0 ||
		   endTimestamp < beginTimestamp){
			complete(GpuFrameTimingStatus::Invalid);
			continue;
		}

		const double seconds =
			static_cast<double>(endTimestamp - beginTimestamp) /
			static_cast<double>(disjoint.Frequency);
		complete(GpuFrameTimingStatus::Resolved, seconds);
	}
}

std::vector<GpuFrameTimingResult>
GraphicsContext::ConsumeResolvedGpuFrameTimings(){
	ResolveGpuFrameTimingQueries();
	std::stable_sort(
		m_ResolvedGpuFrameTimings.begin(),
		m_ResolvedGpuFrameTimings.end(),
		[](const GpuFrameTimingResult& lhs, const GpuFrameTimingResult& rhs){
			return lhs.frameSerial < rhs.frameSerial;
		}
	);

	std::vector<GpuFrameTimingResult> results;
	results.swap(m_ResolvedGpuFrameTimings);
	return results;
}

void GraphicsContext::BeginGpuFrameTiming(uint64_t frameSerial){
	ResolveGpuFrameTimingQueries();
	if(!m_GpuTimingAvailable || !m_DeviceContext){
		m_ResolvedGpuFrameTimings.push_back({
			frameSerial,
			0.0,
			GpuFrameTimingStatus::Dropped
		});
		m_ActiveGpuTimingIndex = -1;
		return;
	}

	auto& timing = m_GpuTimingQueries[m_GpuTimingWriteIndex];
	if(timing.pending){
		// GPUがリング長以上遅れてもCPUを待機させない。
		// 計測できなかったFrameは明示的にDroppedとして返す。
		m_ResolvedGpuFrameTimings.push_back({
			frameSerial,
			0.0,
			GpuFrameTimingStatus::Dropped
		});
		m_ActiveGpuTimingIndex = -1;
		return;
	}

	timing.submittedFrameSerial = frameSerial;
	m_ActiveGpuTimingIndex = static_cast<int>(m_GpuTimingWriteIndex);
	m_DeviceContext->Begin(timing.disjoint.Get());
	m_DeviceContext->End(timing.beginTimestamp.Get());
}

void GraphicsContext::EndGpuFrameTiming(){
	if(!m_GpuTimingAvailable || !m_DeviceContext || m_ActiveGpuTimingIndex < 0){
		return;
	}

	auto& timing = m_GpuTimingQueries[static_cast<size_t>(m_ActiveGpuTimingIndex)];
	m_DeviceContext->End(timing.endTimestamp.Get());
	m_DeviceContext->End(timing.disjoint.Get());
	timing.pending = true;

	m_GpuTimingWriteIndex =
		(m_GpuTimingWriteIndex + 1) % kGpuTimingBufferedFrames;
	m_ActiveGpuTimingIndex = -1;
}

'''
text, count = pattern.subn(replacement, text, count=1)
if count != 1:
    raise RuntimeError("GraphicsContext GPU timing implementation was not found")
write(graphics_cpp, text)


# ---------------------------------------------------------------------------
# TimeService: completed CPU record owns its Frame Serial and Update time.
# ---------------------------------------------------------------------------
time_h = "Source/GameApplication/Service/Runtime/TimeService/timeService.h"
text = read(time_h)
text = replace_once(
    text,
    '#include "buildSetting.h"\n',
    '#include "buildSetting.h"\n#include <cstdint>\n',
    "TimeService cstdint include",
)
text = replace_once(
    text,
    'struct DrawTimingBreakdown {\n',
    'struct DrawTimingBreakdown {\n'
    '\tuint64_t frameSerial = 0;\n'
    '\tdouble update = 0.0;\n',
    "CPU frame record header",
)
text = replace_once(
    text,
    '\tvoid BeginDraw();\n',
    '\tvoid BeginDraw(uint64_t frameSerial);\n',
    "BeginDraw serial signature",
)
write(time_h, text)


time_cpp = "Source/GameApplication/Service/Runtime/TimeService/timeService.cpp"
text = read(time_cpp)
text = replace_once(
    text,
    'void TimeService::BeginDraw(){\n',
    'void TimeService::BeginDraw(uint64_t frameSerial){\n',
    "BeginDraw implementation signature",
)
text = replace_once(
    text,
    '\tdrawSectionActive_ = false;\n'
    '\tcurrentDrawTiming_ = {};\n'
    '}\n\nvoid TimeService::BeginDrawSection',
    '\tdrawSectionActive_ = false;\n'
    '\tcurrentDrawTiming_ = {};\n'
    '\tcurrentDrawTiming_.frameSerial = frameSerial;\n'
    '\tcurrentDrawTiming_.update = deltaUpdateTime_;\n'
    '}\n\nvoid TimeService::BeginDrawSection',
    "BeginDraw CPU frame snapshot",
)
write(time_cpp, text)


# ---------------------------------------------------------------------------
# Editor context carries one-shot GPU results instead of a repeated latest value.
# ---------------------------------------------------------------------------
editor_h = "Source/GameApplication/Engine/Editor/editorService.h"
text = read(editor_h)
text = replace_once(
    text,
    '#include "Runtime/TimeService/timeService.h"\n',
    '#include "Runtime/TimeService/timeService.h"\n'
    '#include "Graphics/GpuFrameTiming.h"\n',
    "Editor GPU timing include",
)
text = replace_once(
    text,
    '\tdouble GPUFrameTime = 0.0;\n'
    '\tbool GPUFrameTimeValid = false;\n',
    '\tconst std::vector<GpuFrameTimingResult>* ResolvedGpuFrameTimings = nullptr;\n',
    "Editor one-shot GPU results",
)
write(editor_h, text)


# ---------------------------------------------------------------------------
# Engine serial lifecycle and completed resize metadata.
# ---------------------------------------------------------------------------
engine_cpp = "Source/GameApplication/Engine/engine.cpp"
text = read(engine_cpp)
text = replace_once(
    text,
    '\tauto initialScene = std::make_shared<Scene>();\n',
    '\tuint64_t drawFrameSerial = 0;\n'
    '\tuint64_t completedResizeSerial = 0;\n'
    '\tdouble completedResizeCpuTime = 0.0;\n\n'
    '\tauto initialScene = std::make_shared<Scene>();\n',
    "Engine frame serial state",
)
text = replace_once(
    text,
    '\t\ttime->BeginDraw();\n',
    '\t\tconst uint64_t activeFrameSerial = ++drawFrameSerial;\n'
    '\t\tconst uint64_t activeResizeSerial = renderer->GetResizeSerial();\n'
    '\t\tconst double activeResizeCpuTime =\n'
    '\t\t\trenderer->GetLastResizeCpuTimeSeconds();\n'
    '\t\ttime->BeginDraw(activeFrameSerial);\n',
    "Engine BeginDraw serial",
)
text = replace_once(
    text,
    '\t\tgraphics->BeginGpuFrameTiming();\n',
    '\t\tgraphics->BeginGpuFrameTiming(activeFrameSerial);\n'
    '\t\tconst auto resolvedGpuFrameTimings =\n'
    '\t\t\tgraphics->ConsumeResolvedGpuFrameTimings();\n',
    "Engine GPU result consumption",
)
text = replace_once(
    text,
    '\t\t\tdraw.UpdateTime = time->GetDeltaUpdateTime();\n'
    '\t\t\tdraw.DrawTime = time->GetDrawTime();\n'
    '\t\t\tdraw.FPS = time->GetFrameFPS();\n'
    '\t\t\tdraw.FixedUpdateFPS = time->GetFixedUpdateFPS();\n'
    '\t\t\tdraw.DrawTiming = time->GetDrawTimingBreakdown();\n'
    '\t\t\tdraw.GPUFrameTime = graphics->GetGpuFrameTimeSeconds();\n'
    '\t\t\tdraw.GPUFrameTimeValid = graphics->HasValidGpuFrameTime();\n',
    '\t\t\tdraw.DrawTiming = time->GetDrawTimingBreakdown();\n'
    '\t\t\tdraw.UpdateTime = draw.DrawTiming.update;\n'
    '\t\t\tdraw.DrawTime = draw.DrawTiming.total;\n'
    '\t\t\tdraw.FPS = time->GetFrameFPS();\n'
    '\t\t\tdraw.FixedUpdateFPS = time->GetFixedUpdateFPS();\n'
    '\t\t\tdraw.ResolvedGpuFrameTimings = &resolvedGpuFrameTimings;\n',
    "Engine completed frame context",
)
text = replace_once(
    text,
    '\t\t\tdraw.ResizeSerial = renderer->GetResizeSerial();\n'
    '\t\t\tdraw.LastResizeCpuTime = renderer->GetLastResizeCpuTimeSeconds();\n',
    '\t\t\tdraw.ResizeSerial = completedResizeSerial;\n'
    '\t\t\tdraw.LastResizeCpuTime = completedResizeCpuTime;\n',
    "Engine completed resize context",
)
text = replace_once(
    text,
    '\t\ttime->EndDraw();\n',
    '\t\ttime->EndDraw();\n'
    '\t\tcompletedResizeSerial = activeResizeSerial;\n'
    '\t\tcompletedResizeCpuTime = activeResizeCpuTime;\n',
    "Engine completed metadata publish",
)
write(engine_cpp, text)


# ---------------------------------------------------------------------------
# Performance Monitor: CPU history keyed by serial, deferred GPU merge,
# status-aware graph and deterministic spike rebuild.
# ---------------------------------------------------------------------------
write(
    "Source/GameApplication/Engine/Editor/UI/PerformanceMonitor.h",
    r'''// =======================================================================
//
// PerformanceMonitor.h
//
// =======================================================================
#pragma once

#include "GameApplication.h"
#include "buildSetting.h"
#define SAMPLE_LENGTH (TARGET_FPS)

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

class PerformanceMonitor: public IEditorUI{
public:
	void Initialize(EditorService* editor) override {
		m_editor = editor;
	}
	void Finalize() override {}
	void Draw(const EditorDrawContext ctx) override;

private:
	struct PanelTimingSampleSeries {
		std::string name;
		std::array<float, SAMPLE_LENGTH> samples{};
	};

	struct FrameTimingRecord {
		uint64_t frame = 0;
		float updateMilliseconds = 0.0f;
		float drawMilliseconds = 0.0f;
		float gpuMilliseconds = 0.0f;
		GpuFrameTimingStatus gpuStatus = GpuFrameTimingStatus::Pending;
		DrawTimingBreakdown drawTiming{};
		float resizeMilliseconds = 0.0f;
		std::string dominantPanel;
		float dominantPanelMilliseconds = 0.0f;
		bool startup = false;
		bool resize = false;
	};

	struct FrameSpikeRecord {
		uint64_t frame = 0;
		float peakMilliseconds = 0.0f;
		float updateMilliseconds = 0.0f;
		float drawMilliseconds = 0.0f;
		float gpuMilliseconds = 0.0f;
		GpuFrameTimingStatus gpuStatus = GpuFrameTimingStatus::Pending;
		float framePacingMilliseconds = 0.0f;
		float renderMilliseconds = 0.0f;
		float editorMilliseconds = 0.0f;
		float presentMilliseconds = 0.0f;
		float unaccountedMilliseconds = 0.0f;
		float resizeMilliseconds = 0.0f;
		float dominantMilliseconds = 0.0f;
		std::string dominantSection;
		std::string dominantPanel;
		float dominantPanelMilliseconds = 0.0f;
		bool startup = false;
		bool resize = false;
	};

	void RecordCompletedFrame(const EditorDrawContext& ctx);
	void ApplyGpuFrameTimings(const EditorDrawContext& ctx);
	void ApplyGpuFrameTiming(const GpuFrameTimingResult& result);
	void RebuildFrameSpikes();

	EditorService* m_editor = nullptr;

	float FixedFpsSamples[SAMPLE_LENGTH]{};
	float DeltaFpsSamples[SAMPLE_LENGTH]{};
	float UpdateSamples[SAMPLE_LENGTH]{};
	float DrawSamples[SAMPLE_LENGTH]{};

	float FramePacingWaitSamples[SAMPLE_LENGTH]{};
	float FrameSetupSamples[SAMPLE_LENGTH]{};
	float ImGuiBeginSamples[SAMPLE_LENGTH]{};
	float RenderScheduleSamples[SAMPLE_LENGTH]{};
	float DebugDrawSamples[SAMPLE_LENGTH]{};
	float EditorUIBuildSamples[SAMPLE_LENGTH]{};
	float ImGuiRenderSamples[SAMPLE_LENGTH]{};
	float PresentSamples[SAMPLE_LENGTH]{};
	float UnaccountedSamples[SAMPLE_LENGTH]{};
	float GPUFrameTimeSamples[SAMPLE_LENGTH]{};
	uint64_t FrameSerialSamples[SAMPLE_LENGTH]{};
	GpuFrameTimingStatus GPUFrameStatusSamples[SAMPLE_LENGTH]{};

	std::vector<PanelTimingSampleSeries> PanelTimingSamples;
	std::deque<FrameTimingRecord> FrameHistory;
	std::map<uint64_t, GpuFrameTimingResult> DeferredGpuResults;
	std::deque<FrameSpikeRecord> FrameSpikes;

	float UsageSamples[SAMPLE_LENGTH]{};
	float CommitSizeSamples[SAMPLE_LENGTH]{};
	float WorkingSetSizeSamples[SAMPLE_LENGTH]{};
	int SampleCount = 0;

	uint64_t LastRecordedFrameSerial = 0;
	uint64_t LastResizeSerial = 0;
	float SpikeThresholdMilliseconds = 20.0f;
	int ResizeGraceFrames = 0;
};
'''
)

write(
    "Source/GameApplication/Engine/Editor/UI/PerformanceMonitor.cpp",
    r'''// =======================================================================
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
'''
)


# ---------------------------------------------------------------------------
# Migration guards.
# ---------------------------------------------------------------------------
checks = {
    graphics_h: [
        "BeginGpuFrameTiming(uint64_t frameSerial)",
        "ConsumeResolvedGpuFrameTimings",
        "submittedFrameSerial",
    ],
    graphics_cpp: [
        "GpuFrameTimingStatus::Dropped",
        "stable_sort",
        "results.swap(m_ResolvedGpuFrameTimings)",
    ],
    time_h: ["frameSerial", "double update", "BeginDraw(uint64_t frameSerial)"],
    engine_cpp: ["activeFrameSerial", "resolvedGpuFrameTimings"],
}
for path, required in checks.items():
    value = read(path)
    for token in required:
        if token not in value:
            raise RuntimeError(f"{path}: required token missing: {token}")

for forbidden in [
    "GetGpuFrameTimeSeconds()",
    "HasValidGpuFrameTime()",
    "GPUFrameTimeValid",
]:
    for path in [graphics_h, graphics_cpp, engine_cpp, editor_h,
                 "Source/GameApplication/Engine/Editor/UI/PerformanceMonitor.cpp"]:
        if forbidden in read(path):
            raise RuntimeError(f"{path}: legacy timing contract remains: {forbidden}")

print("GPU/CPU Frame Serial timing contract migration completed.")
