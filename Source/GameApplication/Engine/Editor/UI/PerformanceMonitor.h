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
#include <string>
#include <vector>

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

// パフォーマンスモニタUI
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

	struct FrameSpikeRecord {
		uint64_t frame = 0;
		float peakMilliseconds = 0.0f;
		float updateMilliseconds = 0.0f;
		float drawMilliseconds = 0.0f;
		float gpuMilliseconds = 0.0f;
		float renderMilliseconds = 0.0f;
		float editorMilliseconds = 0.0f;
		float presentMilliseconds = 0.0f;
		float resizeMilliseconds = 0.0f;
		float dominantMilliseconds = 0.0f;
		std::string dominantSection;
		std::string dominantPanel;
		float dominantPanelMilliseconds = 0.0f;
		bool startup = false;
		bool resize = false;
	};

	void RecordFrameSpike(const EditorDrawContext& ctx);

	EditorService* m_editor = nullptr;

	float FixedFpsSamples[SAMPLE_LENGTH]{};
	float DeltaFpsSamples[SAMPLE_LENGTH]{};
	float UpdateSamples[SAMPLE_LENGTH]{};
	float DrawSamples[SAMPLE_LENGTH]{};

	float FrameSetupSamples[SAMPLE_LENGTH]{};
	float ImGuiBeginSamples[SAMPLE_LENGTH]{};
	float RenderScheduleSamples[SAMPLE_LENGTH]{};
	float DebugDrawSamples[SAMPLE_LENGTH]{};
	float EditorUIBuildSamples[SAMPLE_LENGTH]{};
	float ImGuiRenderSamples[SAMPLE_LENGTH]{};
	float PresentSamples[SAMPLE_LENGTH]{};
	float UnaccountedSamples[SAMPLE_LENGTH]{};
	float GPUFrameTimeSamples[SAMPLE_LENGTH]{};

	std::vector<PanelTimingSampleSeries> PanelTimingSamples;
	std::deque<FrameSpikeRecord> FrameSpikes;

	float UsageSamples[SAMPLE_LENGTH]{};
	float CommitSizeSamples[SAMPLE_LENGTH]{};
	float WorkingSetSizeSamples[SAMPLE_LENGTH]{};
	int SampleCount = 0;

	uint64_t FrameSequence = 0;
	uint64_t LastResizeSerial = 0;
	float SpikeThresholdMilliseconds = 20.0f;
	bool InSpikeSequence = false;
};
