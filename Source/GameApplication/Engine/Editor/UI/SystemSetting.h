// =======================================================================
//
// SystemSetting.h
//
// Project Settings UI。ファイル名とクラス名は既存プロジェクト互換のため維持する。
//
// =======================================================================
#pragma once

#include <cstdio>
#include <string>

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"
#include "Editor/UI/ScheduleProfilerView.h"
#include <filesystem>
#include "Service/Config/configSystem.h"

class SystemSetting : public IEditorUI {
public:
    ~SystemSetting() override { if(m_buildProcess) CloseHandle(m_buildProcess); }
	void Initialize(EditorService* editor) override {
		m_editor = editor;
	}

	void Finalize() override {}
	void Draw(const EditorDrawContext ctx) override;

private:
	EditorService* m_editor = nullptr;
	double m_lastSaveTime = -1000.0;
	ScheduleProfilerViewState m_scheduleViewState;
	std::string m_lastScheduleExportPath;
	std::string m_scheduleExportError;
	double m_lastScheduleExportTime = -1000.0;
    bool PollBuild();
    void StartBuild(BuildTarget target);
    HANDLE m_buildProcess = nullptr;
    std::string m_buildStatus;
    std::filesystem::path m_buildLogPath;
	std::string m_cmakeExecutable = "cmake.exe";
};
