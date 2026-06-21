// =======================================================================
//
// SystemSetting.h
//
// Project Settings UI。ファイル名とクラス名は既存プロジェクト互換のため維持する。
//
// =======================================================================
#pragma once

#include <cstdio>

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"
#include "Editor/UI/ScheduleProfilerView.h"

class SystemSetting : public IEditorUI {
public:
	void Initialize(EditorService* editor) override {
		m_editor = editor;
	}

	void Finalize() override {}
	void Draw(const EditorDrawContext ctx) override;

private:
	EditorService* m_editor = nullptr;
	double m_lastSaveTime = -1000.0;
	ScheduleProfilerViewState m_scheduleViewState;
};
