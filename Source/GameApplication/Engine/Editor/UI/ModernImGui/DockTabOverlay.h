// =======================================================================
//
// DockTabOverlay.h
// Optional semantic glyphs for stock Dear ImGui dock tabs.
//
// Important contract:
// - never redraw tab text or background
// - never modify ContentWidth / RequestedWidth / tab.Width
// - never replace Dear ImGui ellipsis, close-button, focus or hover behavior
// - keep the glyph inside the stock leading region without touching labels
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

			const float fontSize = window->FontRefSize > 0.0f
				? window->FontRefSize
				: context->FontSize;
			const float iconSize = (std::max)(
				10.0f,
				(std::min)(12.0f, fontSize * 0.78f)
			);

			// Vector glyphs intentionally carry roughly 10% internal whitespace.
			// Positioning the nominal bounds at the tab edge yields a readable
			// 10–12 px mark while the visible stroke remains inside the stock
			// leading padding and does not cover the first label glyph.
			const float iconX = tabRect.Min.x + 0.5f;
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
