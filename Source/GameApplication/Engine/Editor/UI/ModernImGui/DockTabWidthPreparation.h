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

			// ContentWidth already contains the original label. Reserve the vector
			// glyph and its gap before DockNode performs this frame's tab layout.
			// Setting the same deterministic value before Begin() prevents the
			// post-layout, next-frame feedback loop that caused width flicker.
			const float requestedWidth = tab.ContentWidth + iconSize + 7.0f;
			tab.RequestedWidth = (std::max)(tab.RequestedWidth, requestedWidth);
			break;
		}
	}
}

} // namespace MImGui
