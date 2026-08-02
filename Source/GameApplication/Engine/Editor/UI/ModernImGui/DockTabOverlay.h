// =======================================================================
//
// DockTabOverlay.h
// Semantic dock-tab glyphs with upstream-compatible clipping and focus color.
//
// =======================================================================
#pragma once

#include <algorithm>

#include <ImGui/imgui_internal.h>

#include "EditorIconWidgets.h"

namespace MImGui {

inline ImVec4 ResolveDockTabSurface(
	const ImGuiTabBar& tabBar,
	bool selected,
	bool hovered
){
	if(hovered){
		return ImGui::GetStyleColorVec4(ImGuiCol_TabHovered);
	}

	const bool focused = (tabBar.Flags & ImGuiTabBarFlags_IsFocused) != 0;
	if(selected){
		return ImGui::GetStyleColorVec4(
			focused ? ImGuiCol_TabActive : ImGuiCol_TabUnfocusedActive
		);
	}
	return ImGui::GetStyleColorVec4(
		focused ? ImGuiCol_Tab : ImGuiCol_TabUnfocused
	);
}

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

		for(ImGuiTabItem& tab : tabBar->Tabs){
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
				9.0f,
				(std::min)(12.0f, fontSize * 0.80f)
			);
			const float gap = 5.0f;
			const bool hasCloseButton =
				(tab.Flags & ImGuiTabItemFlags_NoCloseButton) == 0;

			// Match Dear ImGui's text/close-button contract. The close button may
			// only appear while interacting, but its area is still excluded here so
			// our ellipsis never jumps or collides when hover begins.
			const float closeReserve = hasCloseButton
				? fontSize + tabBar->FramePadding.x
				: 0.0f;
			const float contentMinX =
				tabRect.Min.x + tabBar->FramePadding.x;
			const float contentMaxX =
				tabRect.Max.x - tabBar->FramePadding.x - closeReserve;
			if(contentMaxX <= contentMinX + 4.0f) break;

			ImDrawList* drawList = tabBar->Window->DrawList;
			const ImVec4 surface = ResolveDockTabSurface(
				*tabBar,
				selected,
				hovered
			);
			const ImRect contentRect(
				ImVec2(contentMinX, tabRect.Min.y + 1.0f),
				ImVec2(contentMaxX, tabRect.Max.y)
			);

			// Cover only the standard label area. Tab shape, border, close button,
			// hit target and docking behavior remain entirely owned by Dear ImGui.
			drawList->AddRectFilled(
				contentRect.Min,
				contentRect.Max,
				ImGui::GetColorU32(surface)
			);

			const float centerY = (tabRect.Min.y + tabRect.Max.y) * 0.5f;
			const float iconX = contentMinX + 1.0f;
			const float availableWidth = contentMaxX - contentMinX;
			const bool showIcon = availableWidth >= iconSize + gap + 16.0f;
			float textX = contentMinX;
			if(showIcon){
				DrawEditorIcon(
					drawList,
					icons.Get(iconType),
					ImVec2(iconX, centerY - iconSize * 0.5f),
					iconSize,
					selected ? 0.96f : (hovered ? 0.84f : 0.58f)
				);
				textX = iconX + iconSize + gap;
			}

			const char* label = window->Name;
			const char* labelEnd = ImGui::FindRenderedTextEnd(label);
			const ImVec2 labelSize = ImGui::CalcTextSize(label, labelEnd);
			const ImVec2 textMin(
				textX,
				centerY - labelSize.y * 0.5f
			);
			const ImVec2 textMax(contentMaxX, tabRect.Max.y);
			if(textMax.x > textMin.x + 2.0f){
				drawList->PushClipRect(
					ImVec2(textMin.x, tabRect.Min.y),
					ImVec2(textMax.x, tabRect.Max.y),
					true
				);
				ImGui::RenderTextEllipsis(
					drawList,
					textMin,
					textMax,
					textMax.x,
					label,
					labelEnd,
					&labelSize
				);
				drawList->PopClipRect();
			}
			break;
		}
	}
}

} // namespace MImGui
