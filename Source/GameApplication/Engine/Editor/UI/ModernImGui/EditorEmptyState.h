// =======================================================================
//
// EditorEmptyState.h
// Restrained empty-state presentation for editor work surfaces.
//
// =======================================================================
#pragma once

#include <algorithm>

#include <ImGui/imgui_internal.h>

#include "EditorIconWidgets.h"

namespace MImGui {

inline void DrawInspectorEmptyState(const EditorIconLibrary& icons){
	ImGuiContext* context = GImGui;
	if(!context) return;

	ImGuiWindow* window = ImGui::FindWindowByName("Inspector");
	if(!window || window->LastFrameActive != context->FrameCount ||
		window->Hidden || !window->DrawList){
		return;
	}

	const ImRect clip = window->InnerClipRect;
	const float width = clip.GetWidth();
	const float height = clip.GetHeight();
	if(width < 140.0f || height < 90.0f) return;

	ImDrawList* drawList = window->DrawList;
	const Theme& theme = GetTheme();
	drawList->PushClipRect(clip.Min, clip.Max, true);

	// Cover the fallback text emitted by Inspector without changing Inspector's
	// selection and lifetime logic. This overlay is only drawn when no entity is
	// selected and therefore never obscures editable content.
	drawList->AddRectFilled(
		clip.Min,
		clip.Max,
		ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg))
	);

	const float iconSize = (std::min)(30.0f, width * 0.10f);
	const char* title = "No Selection";
	const char* detail = "Select an entity in Hierarchy to inspect it.";
	const ImVec2 titleSize = ImGui::CalcTextSize(title);
	const ImVec2 detailSize = ImGui::CalcTextSize(detail);
	const float centerX = (clip.Min.x + clip.Max.x) * 0.5f;
	const float blockTop = clip.Min.y + (std::min)(84.0f, height * 0.22f);

	DrawEditorIcon(
		drawList,
		icons.Get(EditorIcon::Inspector),
		ImVec2(centerX - iconSize * 0.5f, blockTop),
		iconSize,
		0.34f
	);

	const float titleY = blockTop + iconSize + 13.0f;
	drawList->AddText(
		ImVec2(centerX - titleSize.x * 0.5f, titleY),
		ImGui::GetColorU32(theme.textSecondary),
		title
	);
	drawList->AddText(
		ImVec2(
			centerX - detailSize.x * 0.5f,
			titleY + titleSize.y + 7.0f
		),
		ImGui::GetColorU32(WithAlpha(theme.textDisabled, 0.88f)),
		detail
	);

	drawList->PopClipRect();
}

} // namespace MImGui
