// =======================================================================
// 
// PerformanceMonitor.h
// 
// =======================================================================
#pragma once

#include "GameApplication.h"
#include "buildSetting.h"
#define SAMPLE_LENGTH (TARGET_FPS)

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

	EditorService* m_editor = nullptr;

	float m_FixedFpsSamples[SAMPLE_LENGTH]{};
	float m_DeltaFpsSamples[SAMPLE_LENGTH]{};
	float m_UpdateSamples[SAMPLE_LENGTH]{};
	float m_DrawSamples[SAMPLE_LENGTH]{};
	float m_UsageSamples[SAMPLE_LENGTH]{};
	float m_CommitSizeSamples[SAMPLE_LENGTH]{};
	float m_WorkingSetSizeSamples[SAMPLE_LENGTH]{};
	int m_SampleCount= 0;
};
