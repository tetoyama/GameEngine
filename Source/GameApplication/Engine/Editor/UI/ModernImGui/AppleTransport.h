// =======================================================================
//
// AppleTransport.h
// Compact transport controls for the editor's global toolbar.
//
// =======================================================================
#pragma once

#include "ModernImGui.h"

namespace MImGui {

enum class TransportGlyph {
	Stop,
	Play,
	Pause,
	Step,
};

inline float TransportButtonWidth(){ return 34.0f; }
inline float TransportButtonHeight(){ return 22.0f; }
inline float TransportGroupWidth(int buttonCount = 3){
	return TransportButtonWidth() * static_cast<float>(buttonCount);
}

inline void DrawTransportGroupBackground(
	const ImVec2& topLeft,
	int buttonCount = 3
){
	const Theme& theme = GetTheme();
	const ImVec2 bottomRight(
		topLeft.x + TransportGroupWidth(buttonCount),
		topLeft.y + TransportButtonHeight()
	);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(
		topLeft,
		bottomRight,
		ImGui::GetColorU32(WithAlpha(theme.raised, 0.92f)),
		TransportButtonHeight() * 0.5f
	);
	drawList->AddRect(
		topLeft,
		bottomRight,
		ImGui::GetColorU32(EffectiveOutline()),
		TransportButtonHeight() * 0.5f,
		0,
		EffectiveStrokeWidth()
	);
	DrawMaterialEdge(
		drawList,
		topLeft,
		bottomRight,
		TransportButtonHeight() * 0.5f,
		0.72f
	);
}

inline void DrawTransportGlyph(
	ImDrawList* drawList,
	TransportGlyph glyph,
	const ImVec2& center,
	ImU32 color
){
	switch(glyph){
		case TransportGlyph::Stop:
			drawList->AddRectFilled(
				ImVec2(center.x - 4.0f, center.y - 4.0f),
				ImVec2(center.x + 4.0f, center.y + 4.0f),
				color,
				1.5f
			);
			break;
		case TransportGlyph::Play:{
			const ImVec2 points[3] = {
				ImVec2(center.x - 3.5f, center.y - 5.0f),
				ImVec2(center.x - 3.5f, center.y + 5.0f),
				ImVec2(center.x + 5.0f, center.y),
			};
			drawList->AddTriangleFilled(points[0], points[1], points[2], color);
			break;
		}
		case TransportGlyph::Pause:
			drawList->AddRectFilled(
				ImVec2(center.x - 4.5f, center.y - 5.0f),
				ImVec2(center.x - 1.5f, center.y + 5.0f),
				color,
				1.0f
			);
			drawList->AddRectFilled(
				ImVec2(center.x + 1.5f, center.y - 5.0f),
				ImVec2(center.x + 4.5f, center.y + 5.0f),
				color,
				1.0f
			);
			break;
		case TransportGlyph::Step:{
			const ImVec2 points[3] = {
				ImVec2(center.x - 5.0f, center.y - 5.0f),
				ImVec2(center.x - 5.0f, center.y + 5.0f),
				ImVec2(center.x + 2.0f, center.y),
			};
			drawList->AddTriangleFilled(points[0], points[1], points[2], color);
			drawList->AddRectFilled(
				ImVec2(center.x + 3.0f, center.y - 5.0f),
				ImVec2(center.x + 5.0f, center.y + 5.0f),
				color,
				0.8f
			);
			break;
		}
	}
}

inline bool TransportButton(
	const char* id,
	TransportGlyph glyph,
	bool selected,
	bool enabled,
	bool drawTrailingSeparator,
	const char* tooltip
){
	if(!id) id = "TransportButton";
	if(!enabled) ImGui::BeginDisabled();
	const bool pressed = ImGui::InvisibleButton(
		id,
		ImVec2(TransportButtonWidth(), TransportButtonHeight())
	);
	if(!enabled) ImGui::EndDisabled();

	const ImGuiID itemID = ImGui::GetItemID();
	const bool hovered = enabled && ImGui::IsItemHovered();
	const bool held = enabled && ImGui::IsItemActive();
	const bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavActive;
	const float hoverAmount = Animate(itemID ^ 0x1659E8C1u, hovered ? 1.0f : 0.0f);
	const float selectedAmount = Animate(itemID ^ 0x7A2D40B3u, selected ? 1.0f : 0.0f);
	const float pressAmount = AnimateInteractive(
		itemID ^ 0x31C65F89u,
		held,
		0.68f,
		26.0f,
		1.0f
	);

	const ImVec2 boundsMin = ImGui::GetItemRectMin();
	const ImVec2 boundsMax = ImGui::GetItemRectMax();
	const ImVec2 center(
		(boundsMin.x + boundsMax.x) * 0.5f,
		(boundsMin.y + boundsMax.y) * 0.5f
	);
	const Theme& theme = GetTheme();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	ImVec4 fill = Lerp(
		WithAlpha(theme.panel, 0.0f),
		WithAlpha(theme.hover, 0.64f),
		hoverAmount
	);
	fill = Lerp(fill, WithAlpha(theme.accent, 0.28f), selectedAmount);
	fill = Lerp(fill, WithAlpha(theme.pressed, 0.70f), pressAmount * 0.34f);
	if(fill.w > 0.001f){
		drawList->AddRectFilled(boundsMin, boundsMax, ImGui::GetColorU32(fill));
	}

	if(drawTrailingSeparator){
		drawList->AddLine(
			ImVec2(boundsMax.x, boundsMin.y + 5.0f),
			ImVec2(boundsMax.x, boundsMax.y - 5.0f),
			ImGui::GetColorU32(WithAlpha(theme.separator, 0.70f)),
			1.0f
		);
	}
	if(focused){
		DrawFocusRing(
			drawList,
			ImVec2(boundsMin.x + 2.0f, boundsMin.y + 2.0f),
			ImVec2(boundsMax.x - 2.0f, boundsMax.y - 2.0f),
			6.0f
		);
	}

	const ImVec4 glyphColor = !enabled
		? theme.textDisabled
		: (selected || hovered ? theme.textPrimary : theme.textSecondary);
	DrawTransportGlyph(drawList, glyph, center, ImGui::GetColorU32(glyphColor));

	if(tooltip && tooltip[0] && hovered){
		ImGui::SetTooltip("%s", tooltip);
	}
	return enabled && pressed;
}

} // namespace MImGui
