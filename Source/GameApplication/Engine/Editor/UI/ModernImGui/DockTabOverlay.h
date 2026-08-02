// =======================================================================
//
// DockTabOverlay.h
// Optional semantic glyphs for stock Dear ImGui dock tabs.
//
// Important contract:
// - never redraw tab text or background
// - never modify ContentWidth / RequestedWidth / tab.Width
// - never replace Dear ImGui ellipsis, close-button, focus or hover behavior
// - draw a glyph only in genuinely unused trailing space
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
				8.0f,
				(std::min)(11.0f, fontSize * 0.72f)
			);
			const float labelGap = 5.0f;
			const float rightSafety = 3.0f;

			const char* label = window->Name;
			const char* labelEnd = ImGui::FindRenderedTextEnd(label);
			const float labelWidth = ImGui::CalcTextSize(label, labelEnd).x;
			const float labelStartX = tabRect.Min.x + tabBar->FramePadding.x;
			const float labelEndX = labelStartX + labelWidth;

			// A closeable tab may reveal its close button on hover. Always protect
			// that hit target, but do not take any width away from the stock label:
			// the glyph is optional and simply disappears when the spare region is
			// too small.
			const bool hasCloseButton =
				(tab.Flags & ImGuiTabItemFlags_NoCloseButton) == 0;
			const float trailingLimit = tabRect.Max.x - tabBar->FramePadding.x -
				(hasCloseButton ? fontSize : 0.0f) - rightSafety;
			const float iconX = labelEndX + labelGap;

			if(iconX + iconSize > trailingLimit){
				break;
			}

			const float centerY = (tabRect.Min.y + tabRect.Max.y) * 0.5f;
			const bool focused = (tabBar->Flags & ImGuiTabBarFlags_IsFocused) != 0;
			const float alpha = selected
				? (focused ? 0.88f : 0.64f)
				: (hovered ? 0.72f : 0.46f);

			DrawEditorIcon(
				tabBar->Window->DrawList,
				icons.Get(iconType),
				ImVec2(iconX, centerY - iconSize * 0.5f),
				iconSize,
				alpha
			);
			break;
		}
	}
}

} // namespace MImGui
