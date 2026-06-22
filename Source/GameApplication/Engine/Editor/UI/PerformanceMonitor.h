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

	float UsageSamples[SAMPLE_LENGTH]{};
	float CommitSizeSamples[SAMPLE_LENGTH]{};
	float WorkingSetSizeSamples[SAMPLE_LENGTH]{};
	int SampleCount = 0;
};
