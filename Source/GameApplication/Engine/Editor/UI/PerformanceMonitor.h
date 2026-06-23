// =======================================================================
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
