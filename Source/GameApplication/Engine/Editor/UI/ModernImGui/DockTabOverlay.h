// =======================================================================
//
// DockTabOverlay.h
// Optional semantic glyphs for stock Dear ImGui dock tabs.
//
// Important contract:
// - never redraw tab text or background
// - never modify ContentWidth / RequestedWidth / tab.Width
// - never replace Dear ImGui ellipsis, close-button, focus or hover behavior
// - draw only inside the stock leading frame-padding region
//
// =======================================================================
#pragma once

#include <algorithm>

#include <ImGui/imgui_internal.h>

#include "EditorIconWidgets.h"

namespace MImGui {

inline void DrawDockTabOverlay(const EditorIconLibrary& icons){
	ImGuiContext* context = GImGui;
	if(!context) return;

	for(ImGuiWindow* window : context->Windows){
		if(!window || window->LastFrameActive != context->FrameCount ||
		   !window->DockNode){
			continue;
		}

		const EditorIcon iconType = ResolveDockTabIcon(window);
		if(iconType == EditorIcon::Count) continue;

		ImGuiTabBar* tabBar = window->DockNode->TabBar;
		if(!tabBar || !tabBar->Window || !tabBar->Window->DrawList) continue;

		for(const ImGuiTabItem& tab : tabBar->Tabs){
			if(tab.Window != window || tab.Width <= 1.0f) continue;

			const float tabMinX =
				tabBar->BarRect.Min.x + tab.Offset - tabBar->ScrollingAnim;
			const float tabMaxX = tabMinX + tab.Width;
			if(tabMaxX <= tabBar->BarRect.Min.x ||
			   tabMinX >= tabBar->BarRect.Max.x){
				break;
			}

			const ImRect tabRect(
				ImVec2(tabMinX, tabBar->BarRect.Min.y),
				ImVec2(tabMaxX, tabBar->BarRect.Max.y)
			);
			const bool selected = tab.ID == tabBar->SelectedTabId;
			const bool hovered = ImGui::IsMouseHoveringRect(
				tabRect.Min,
				tabRect.Max,
				false
			);

			// Use only the padding Dear ImGui already reserved before the label.
			// This keeps the standard label, ellipsis and close-button geometry
			// completely untouched while making the glyph deterministic.
			const float leadingPadding = tabBar->FramePadding.x;
			if(leadingPadding < 4.0f) break;

			const float iconSize = (std::max)(
				4.0f,
				(std::min)(7.0f, leadingPadding - 1.0f)
			);
			const float iconX = tabRect.Min.x +
				(std::max)(0.5f, (leadingPadding - iconSize) * 0.5f);
			const float centerY = (tabRect.Min.y + tabRect.Max.y) * 0.5f;

			const bool focused =
				(tabBar->Flags & ImGuiTabBarFlags_IsFocused) != 0;
			const float alpha = selected
				? (focused ? 0.92f : 0.68f)
				: (hovered ? 0.76f : 0.50f);

			ImDrawList* drawList = tabBar->Window->DrawList;
			drawList->PushClipRect(tabRect.Min, tabRect.Max, true);
			DrawEditorIcon(
				drawList,
				icons.Get(iconType),
				ImVec2(iconX, centerY - iconSize * 0.5f),
				iconSize,
				alpha
			);
			drawList->PopClipRect();
			break;
		}
	}
}

} // namespace MImGui
