#pragma once

#include "GameApplication.h"
#define SAMPLE_LENGTH (TARGET_FPS)

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

class DebugWindow : public IEditorUI{

public:
	void Initialize() override {}
	void Finalize() override {}
	void Draw(EditorDrawContext ctx) override;

private:
	float FixedFpsSamples[SAMPLE_LENGTH]{};
	float DeltaFpsSamples[SAMPLE_LENGTH]{};
	float UpdateSamples[SAMPLE_LENGTH]{};
	float DrawSamples[SAMPLE_LENGTH]{};
	float UsageSamples[SAMPLE_LENGTH]{};
	float CommitSizeSamples[SAMPLE_LENGTH]{};
	float WorkingSetSizeSamples[SAMPLE_LENGTH]{};
	int SampleCount = 0;
};
