// =======================================================================
//
// DockTabLayout.h
// Width reservation for editor glyphs overlaid on Dear ImGui dock tabs.
//
// =======================================================================
#pragma once

#include <algorithm>

#include <ImGui/imgui_internal.h>

#include "EditorIconWidgets.h"

namespace MImGui {

inline void ReserveDockTabOverlayWidths(){
	ImGuiContext* context = GImGui;
	if(!context) return;

	for(ImGuiWindow* window : context->Windows){
		if(!window || window->LastFrameActive != context->FrameCount || !window->DockNode){
			continue;
		}

		const EditorIcon iconType = ResolveDockTabIcon(window);
		if(iconType == EditorIcon::Count) continue;

		ImGuiTabBar* tabBar = window->DockNode->TabBar;
		if(!tabBar) continue;

		for(ImGuiTabItem& tab : tabBar->Tabs){
			if(tab.Window != window || tab.Width <= 0.0f) continue;

			const float fontSize = window->FontRefSize > 0.0f
				? window->FontRefSize
				: context->FontSize;
			const float iconSize = (std::max)(
				9.0f,
				(std::min)(12.0f, fontSize * 0.80f)
			);
			const bool hasCloseButton =
				(tab.Flags & ImGuiTabItemFlags_NoCloseButton) == 0;
			const float closeReserve = hasCloseButton
				? fontSize + tabBar->FramePadding.x * 2.0f + 3.0f
				: tabBar->FramePadding.x + 3.0f;

			const char* label = window->Name;
			const char* labelEnd = ImGui::FindRenderedTextEnd(label);
			const float labelWidth = ImGui::CalcTextSize(label, labelEnd).x;

			// Keep this equation identical to DrawDockTabIcons():
			// left padding + one-pixel icon inset + icon + gap + label +
			// close-button reserve. RequestedWidth is raised only by the measured
			// deficit, so an already-correct tab never grows again.
			const float textStartFromTabMin =
				tabBar->FramePadding.x + 1.0f + iconSize + 5.0f;
			const float availableLabelWidth =
				tab.Width - textStartFromTabMin - closeReserve;
			const float missingWidth = labelWidth - availableLabelWidth;
			if(missingWidth > 0.25f){
				const float requestedWidth = tab.Width + missingWidth + 2.0f;
				tab.RequestedWidth = (std::max)(tab.RequestedWidth, requestedWidth);
			}
			break;
		}
	}
}

} // namespace MImGui
