// =======================================================================
//
// DockTabWidthPreparation.h
// Prepares semantic dock-tab widths before any editor window begins.
//
// =======================================================================
#pragma once

#include <algorithm>

#include <ImGui/imgui_internal.h>

#include "EditorIconLibrary.h"

namespace MImGui {

inline EditorIcon ResolvePreparedDockTabIcon(const ImGuiWindow* window){
	if(!window) return EditorIcon::Count;
	const ImGuiID id = window->ID;
	if(id == ImHashStr("Hierarchy")) return EditorIcon::Hierarchy;
	if(id == ImHashStr("Editor View")) return EditorIcon::Viewport;
	if(id == ImHashStr("Play View") || id == ImHashStr("Player View")) return EditorIcon::Play;
	if(id == ImHashStr("Inspector")) return EditorIcon::Inspector;
	if(id == ImHashStr("Assets Browser")) return EditorIcon::Assets;
	if(id == ImHashStr("Debug Log")) return EditorIcon::Console;
	if(id == ImHashStr("Performance Monitor")) return EditorIcon::Performance;
	if(id == ImHashStr("Project Settings")) return EditorIcon::Settings;
	if(id == ImHashStr("Scene Settings")) return EditorIcon::Scene;
	if(id == ImHashStr("Scene Storage Runtime Telemetry")) return EditorIcon::Telemetry;
	return EditorIcon::Count;
}

inline void PrepareDockTabWidths(){
	ImGuiContext* context = GImGui;
	if(!context) return;

	for(ImGuiWindow* window : context->Windows){
		if(!window || !window->DockNode) continue;
		if(ResolvePreparedDockTabIcon(window) == EditorIcon::Count) continue;

		ImGuiTabBar* tabBar = window->DockNode->TabBar;
		if(!tabBar) continue;

		for(ImGuiTabItem& tab : tabBar->Tabs){
			if(tab.Window != window) continue;

			const float fontSize = window->FontRefSize > 0.0f
				? window->FontRefSize
				: context->FontSize;
			const float iconSize = (std::max)(
				9.0f,
				(std::min)(12.0f, fontSize * 0.80f)
			);
			const bool hasCloseButton =
				(tab.Flags & ImGuiTabItemFlags_NoCloseButton) == 0;

			// TabItemCalcSize follows Dear ImGui's own width contract and already
			// includes the label, frame padding and optional close-button space.
			// ContentWidth is what TabBarLayout consumes before this frame's Begin()
			// calls. Add only the semantic glyph, its gap and a small clip guard.
			const float stockWidth = ImGui::TabItemCalcSize(
				window->Name,
				hasCloseButton
			).x;
			constexpr float iconGap = 5.0f;
			constexpr float clipGuard = 3.0f;
			tab.ContentWidth = stockWidth + iconSize + iconGap + clipGuard;

			// A post-draw compatibility write may still exist in the overlay path.
			// Clear it before layout so it never competes with ContentWidth.
			tab.RequestedWidth = -1.0f;
			break;
		}
	}
}

} // namespace MImGui
