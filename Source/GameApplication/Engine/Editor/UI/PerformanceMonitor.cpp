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

	double m_Fps= ctx.fps;
	double m_FixedFps= ctx.fixedUpdateFps;
	double m_Draw= ctx.drawTime;
	double m_Update= ctx.updateTime;
	bool* showPerformanceMonitor = &m_pEditor->GetUI<MenuBar>()->showPerformanceMonitor;

	HANDLE hProc= GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS_EX pmc;
	BOOL isSuccess= GetProcessMemoryInfo(
		hProc,
		(PROCESS_MEMORY_COUNTERS*)&pmc,
		sizeof(pmc));
	CloseHandle(hProc);
	if (isSuccess == FALSE) {
		exit(EXIT_FAILURE);
	}

	m_SampleCount = (m_SampleCount + 1) % TARGET_FPS;
	if (m_SampleCount == 0) {
		for (int n = 0; n < SAMPLE_LENGTH - 1; n++) {
			m_FixedFpsSamples[n] = m_FixedFpsSamples[n + 1];
			m_DeltaFpsSamples[n] = m_DeltaFpsSamples[n + 1];

			m_UsageSamples[n] = m_UsageSamples[n + 1];
			m_CommitSizeSamples[n] = m_CommitSizeSamples[n + 1];
			m_WorkingSetSizeSamples[n] = m_WorkingSetSizeSamples[n + 1];
		}
		m_FixedFpsSamples[SAMPLE_LENGTH - 1] = (float)m_FixedFps;
		m_DeltaFpsSamples[SAMPLE_LENGTH - 1] = (float)m_Fps;

		m_UsageSamples[SAMPLE_LENGTH - 1] = 100.0f * pmc.WorkingSetSize / (pmc.WorkingSetSize + pmc.PagefileUsage);
		m_CommitSizeSamples[SAMPLE_LENGTH - 1] = pmc.PagefileUsage / 1000000.0f;
		m_WorkingSetSizeSamples[SAMPLE_LENGTH - 1] = pmc.WorkingSetSize / 1000000.0f;
	}
	for (int n = 0; n < SAMPLE_LENGTH - 1; n++) {
		m_UpdateSamples[n] = m_UpdateSamples[n + 1];
		m_DrawSamples[n] = m_DrawSamples[n + 1];
	}
	m_UpdateSamples[SAMPLE_LENGTH - 1] = (float)m_Update;
	m_DrawSamples[SAMPLE_LENGTH - 1] = (float)m_Draw;

	if(!showPerformanceMonitor || !*showPerformanceMonitor) {
		return;
	}

	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&windowClass);

	//ImGuiWindowFlags toolbar_window_flags = ImGuiWindowFlags_NoCollapse;
	ImGuiWindowFlags toolbarWindowFlags= 0;

	ImGui::Begin("Performance Monitor", showPerformanceMonitor, toolbarWindowFlags);

	if (ImGui::TreeNodeEx("負荷計測", ImGuiTreeNodeFlags_DefaultOpen)) {

			float m_FixedFpsavg= 0.0f;
			float m_DeltaFpsavg= 0.0f;
			float m_UpdateAvg= 0.0f;
			float m_DrawAvg= 0.0f;
			int m_Fpscount= 0;
			for (int n = 0; n < SAMPLE_LENGTH; n++) {
				if (0 < m_FixedFpsSamples[n]) {
					m_FixedFpsavg += m_FixedFpsSamples[n];
					m_DeltaFpsavg += m_DeltaFpsSamples[n];
					m_Fpscount++;
				}
				m_UpdateAvg += m_UpdateSamples[n];
				m_DrawAvg += m_DrawSamples[n];
			}
			if (0 < m_Fpscount) {
				m_FixedFpsavg /= m_Fpscount;
				m_DeltaFpsavg /= m_Fpscount;
			}
			m_UpdateAvg /= SAMPLE_LENGTH;
			m_DrawAvg /= SAMPLE_LENGTH;

			char m_Texts[64]{};
			ImGui::Text("-FPS計測-");
			sprintf(m_Texts, "Fixed:%.2f Avg:%.2f", m_FixedFpsSamples[SAMPLE_LENGTH - 1], m_FixedFpsavg);
			ImGui::Text(m_Texts);
			ImGui::PlotLines("##Fixed", m_FixedFpsSamples, SAMPLE_LENGTH, 0, "", 0.0f);
			sprintf(m_Texts, "Delta:%.2f Avg:%.2f", m_DeltaFpsSamples[SAMPLE_LENGTH - 1], m_DeltaFpsavg);
			ImGui::Text(m_Texts);
			ImGui::PlotLines("##Delta", m_DeltaFpsSamples, SAMPLE_LENGTH, 0, "", 0.0f);
			ImGui::Text("-更新処理-");
			sprintf(m_Texts, "Update:Avg:%.4fms", m_UpdateAvg);
			ImGui::PlotLines(m_Texts, m_UpdateSamples, SAMPLE_LENGTH, 0, "", 0.0f, 1000.0f / 60.0f);
			ImGui::Text("-描画処理-");
			sprintf(m_Texts, "Draw:Avg:%.4fms", m_DrawAvg);
			ImGui::PlotLines(m_Texts, m_DrawSamples, SAMPLE_LENGTH, 0, "", 0.0f, 1000.0f / 60.0f);
			ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("-メモリ使用量-", ImGuiTreeNodeFlags_DefaultOpen)) {
			float usageAvg= 0.0f;
			int fpscount= 0;

			for (int n = 0; n < SAMPLE_LENGTH; n++) {
				if (0 < m_FixedFpsSamples[n]) {
					usageAvg += m_UsageSamples[n];
					fpscount++;
				}
			}
			if (0 < usageAvg) {
				usageAvg /= fpscount;
			}
			char texts[64]{};
			sprintf(texts, "usage:Avg:%.2f%%", usageAvg);
			ImGui::PlotLines(texts, m_UsageSamples, SAMPLE_LENGTH, 0, "", 0.0f, 100.0f);
			sprintf(texts, "Commit:%dMB", (int)(pmc.PagefileUsage / 1000000));
			ImGui::PlotLines(texts, m_CommitSizeSamples, SAMPLE_LENGTH, 0, "", 0.0f, pmc.PeakPagefileUsage / 1000000.0f);
			sprintf(texts, "Working:%dMB", (int)(pmc.WorkingSetSize / 1000000));
			ImGui::PlotLines(texts, m_WorkingSetSizeSamples, SAMPLE_LENGTH, 0, "", 0.0f, pmc.PeakWorkingSetSize / 1000000.0f);
			ImGui::TreePop();
	}
	ImGui::End();
}
