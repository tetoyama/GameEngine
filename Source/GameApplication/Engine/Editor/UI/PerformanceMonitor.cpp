// =======================================================================
// 
// PerformanceMonitor.cpp
// 
// =======================================================================
#include "PerformanceMonitor.h"
#include <Psapi.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"

void PerformanceMonitor::Draw(const EditorDrawContext ctx) {

	double m_Fps= ctx.FPS;
	double m_FixedFps= ctx.FixedUpdateFPS;
	double m_Draw= ctx.DrawTime;
	double m_Update= ctx.UpdateTime;
	bool* showPerformanceMonitor = &m_editor->GetUI<MenuBar>()->showPerformanceMonitor;

	HANDLE m_HProc= GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS_EX m_Pmc;
	BOOL m_IsSuccess= GetProcessMemoryInfo(
		hProc,
		(PROCESS_MEMORY_COUNTERS*)&pmc,
		sizeof(pmc));
	CloseHandle(hProc);
	if (isSuccess == FALSE) {
		exit(EXIT_FAILURE);
	}

	SampleCount = (SampleCount + 1) % TARGET_FPS;
	if (SampleCount == 0) {
		for (int n = 0; n < SAMPLE_LENGTH - 1; n++) {
			FixedFpsSamples[n] = FixedFpsSamples[n + 1];
			DeltaFpsSamples[n] = DeltaFpsSamples[n + 1];

			UsageSamples[n] = UsageSamples[n + 1];
			CommitSizeSamples[n] = CommitSizeSamples[n + 1];
			WorkingSetSizeSamples[n] = WorkingSetSizeSamples[n + 1];
		}
		FixedFpsSamples[SAMPLE_LENGTH - 1] = (float)FixedFPS;
		DeltaFpsSamples[SAMPLE_LENGTH - 1] = (float)FPS;

		UsageSamples[SAMPLE_LENGTH - 1] = 100.0f * pmc.WorkingSetSize / (pmc.WorkingSetSize + pmc.PagefileUsage);
		CommitSizeSamples[SAMPLE_LENGTH - 1] = pmc.PagefileUsage / 1000000.0f;
		WorkingSetSizeSamples[SAMPLE_LENGTH - 1] = pmc.WorkingSetSize / 1000000.0f;
	}
	for (int n = 0; n < SAMPLE_LENGTH - 1; n++) {
		UpdateSamples[n] = UpdateSamples[n + 1];
		DrawSamples[n] = DrawSamples[n + 1];
	}
	UpdateSamples[SAMPLE_LENGTH - 1] = (float)Update;
	DrawSamples[SAMPLE_LENGTH - 1] = (float)Draw;

	if(!showPerformanceMonitor || !*showPerformanceMonitor) {
		return;
	}

	ImGuiWindowClass m_WindowClass;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);

	//ImGuiWindowFlags toolbar_window_flags = ImGuiWindowFlags_NoCollapse;
	ImGuiWindowFlags m_ToolbarWindowFlags= 0;

	ImGui::Begin("Performance Monitor", showPerformanceMonitor, toolbar_window_flags);

	if (ImGui::TreeNodeEx("負荷計測", ImGuiTreeNodeFlags_DefaultOpen)) {

			float m_FixedFpsavg= 0.0f;
			float m_DeltaFpsavg= 0.0f;
			float m_UpdateAvg= 0.0f;
			float m_DrawAvg= 0.0f;
			int m_Fpscount= 0;
			for (int n = 0; n < SAMPLE_LENGTH; n++) {
				if (0 < FixedFpsSamples[n]) {
					FixedFPSAvg += FixedFpsSamples[n];
					DeltaFPSAvg += DeltaFpsSamples[n];
					FPSCount++;
				}
				UpdateAvg += UpdateSamples[n];
				DrawAvg += DrawSamples[n];
			}
			if (0 < FixedFPSAvg) {
				FixedFPSAvg /= FPSCount;
				DeltaFPSAvg /= FPSCount;
			}
			UpdateAvg /= SAMPLE_LENGTH;
			DrawAvg /= SAMPLE_LENGTH;

			char m_Texts[64]{};
			ImGui::Text("-FPS計測-");
			sprintf(Texts, "Fixed:%.2f Avg:%.2f", FixedFpsSamples[SAMPLE_LENGTH - 1], FixedFPSAvg);
			ImGui::Text(Texts);
			ImGui::PlotLines("##Fixed", FixedFpsSamples, SAMPLE_LENGTH, 0, "", 0.0f);
			sprintf(Texts, "Delta:%.2f Avg:%.2f", DeltaFpsSamples[SAMPLE_LENGTH - 1], DeltaFPSAvg);
			ImGui::Text(Texts);
			ImGui::PlotLines("##Delta", DeltaFpsSamples, SAMPLE_LENGTH, 0, "", 0.0f);
			ImGui::Text("-更新処理-");
			sprintf(Texts, "Update:Avg:%.4fms", UpdateAvg);
			ImGui::PlotLines(Texts, UpdateSamples, SAMPLE_LENGTH, 0, "", 0.0f, 1000.0f / 60.0f);
			ImGui::Text("-描画処理-");
			sprintf(Texts, "Draw:Avg:%.4fms", DrawAvg);
			ImGui::PlotLines(Texts, DrawSamples, SAMPLE_LENGTH, 0, "", 0.0f, 1000.0f / 60.0f);
			ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("-メモリ使用量-", ImGuiTreeNodeFlags_DefaultOpen)) {
			float m_UsageAvg= 0.0f;
			int m_Fpscount= 0;

			for (int n = 0; n < SAMPLE_LENGTH; n++) {
				if (0 < FixedFpsSamples[n]) {
					UsageAvg += UsageSamples[n];
					FPSCount++;
				}
			}
			if (0 < UsageAvg) {
				UsageAvg /= FPSCount;
			}
			char m_Texts[64]{};
			sprintf(Texts, "usage:Avg:%.2f%%", UsageAvg);
			ImGui::PlotLines(Texts, UsageSamples, SAMPLE_LENGTH, 0, "", 0.0f, 100.0f);
			sprintf(Texts, "Commit:%dMB", (int)(pmc.PagefileUsage / 1000000));
			ImGui::PlotLines(Texts, CommitSizeSamples, SAMPLE_LENGTH, 0, "", 0.0f, pmc.PeakPagefileUsage / 1000000.0f);
			sprintf(Texts, "Working:%dMB", (int)(pmc.WorkingSetSize / 1000000));
			ImGui::PlotLines(Texts, WorkingSetSizeSamples, SAMPLE_LENGTH, 0, "", 0.0f, pmc.PeakWorkingSetSize / 1000000.0f);
			ImGui::TreePop();
	}
	ImGui::End();
}
