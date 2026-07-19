// =======================================================================
//
// EditorIconWidgets.h
// Icon-aware wrappers layered on top of ModernImGui controls.
//
// =======================================================================
#pragma once

#include <string>

#include "EditorIconLibrary.h"
#include "ModernImGui.h"

namespace MImGui {

inline void DrawEditorIcon(
	const EditorIconImage& icon,
	const ImVec2& topLeft,
	float size,
	float alpha = 1.0f
){
	if(!icon.IsValid() || size <= 0.0f) return;
	ImGui::GetWindowDrawList()->AddImage(
		icon.texture,
		topLeft,
		ImVec2(topLeft.x + size, topLeft.y + size),
		icon.uv0,
		icon.uv1,
		ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha))
	);
}

inline EditorIconImage TextureIcon(TextureData* texture){
	EditorIconImage image;
	if(!texture || !texture->pTexture) return image;
	image.texture._TexID = (ImTextureID)texture->pTexture.Get();
	return image;
}

inline bool IconOnlyButton(
	const char* id,
	const EditorIconImage& icon,
	bool selected = false,
	const char* tooltip = nullptr,
	float size = 24.0f
){
	if(!id) id = "IconOnlyButton";
	const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
	const ImGuiID itemID = ImGui::GetItemID();
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive();
	const bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavActive;

	const float hoverAmount = Animate(itemID ^ 0x414D4E11u, hovered ? 1.0f : 0.0f);
	const float selectedAmount = Animate(itemID ^ 0x315C9D27u, selected ? 1.0f : 0.0f);
	const float pressAmount = AnimateInteractive(itemID ^ 0x7890B4A3u, held, 0.62f, 25.0f, 1.0f);

	const ImVec2 boundsMin = ImGui::GetItemRectMin();
	const ImVec2 boundsMax = ImGui::GetItemRectMax();
	Theme& theme = GetTheme();

	ImVec4 fill = Lerp(
		WithAlpha(theme.panel, 0.0f),
		WithAlpha(theme.hover, 0.52f),
		hoverAmount
	);
	fill = Lerp(fill, WithAlpha(theme.accent, 0.12f), selectedAmount);
	fill = Lerp(fill, WithAlpha(theme.pressed, 0.70f), pressAmount * 0.30f);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if(fill.w > 0.001f){
		drawList->AddRectFilled(
			boundsMin,
			boundsMax,
			ImGui::GetColorU32(fill),
			theme.cornerRadius
		);
	}
	if(selectedAmount > 0.001f){
		drawList->AddRectFilled(
			ImVec2(boundsMin.x + 5.0f, boundsMax.y - 2.5f),
			ImVec2(boundsMax.x - 5.0f, boundsMax.y - 1.0f),
			ImGui::GetColorU32(WithAlpha(theme.accentHover, 0.95f * selectedAmount)),
			1.0f
		);
	}
	if(focused) DrawFocusRing(drawList, boundsMin, boundsMax, theme.cornerRadius);

	const float iconSize = (std::min)(16.0f, size - 6.0f);
	DrawEditorIcon(
		icon,
		ImVec2(
			(boundsMin.x + boundsMax.x - iconSize) * 0.5f,
			(boundsMin.y + boundsMax.y - iconSize) * 0.5f
		),
		iconSize,
		hovered || selected ? 1.0f : 0.68f
	);

	if(tooltip && tooltip[0] && hovered){
		ImGui::SetTooltip("%s", tooltip);
	}
	return pressed;
}

inline bool IconButton(
	const char* id,
	const char* visibleLabel,
	const EditorIconImage& icon,
	const ImVec2& size = ImVec2(0.0f, 0.0f),
	ButtonKind kind = ButtonKind::Secondary,
	float iconSize = 16.0f
){
	if(!id) id = "IconButton";
	if(!visibleLabel) visibleLabel = "";

	std::string label = "   ";
	label += visibleLabel;
	label += "##";
	label += id;

	const bool pressed = Button(label.c_str(), size, kind);
	const ImVec2 boundsMin = ImGui::GetItemRectMin();
	const ImVec2 boundsMax = ImGui::GetItemRectMax();
	const float centerY = (boundsMin.y + boundsMax.y) * 0.5f;
	const float alpha = ImGui::IsItemHovered() || ImGui::IsItemActive()
		? 1.0f
		: 0.84f;
	DrawEditorIcon(
		icon,
		ImVec2(boundsMin.x + 9.0f, centerY - iconSize * 0.5f),
		iconSize,
		alpha
	);
	return pressed;
}

inline bool IconSectionHeader(
	const char* id,
	const char* visibleLabel,
	const EditorIconImage& icon,
	bool* open,
	float width = -1.0f
){
	if(!id) id = "IconSection";
	if(!visibleLabel) visibleLabel = "";

	std::string label = "   ";
	label += visibleLabel;
	label += "##";
	label += id;

	const bool pressed = SectionHeader(label.c_str(), open, width);
	const ImVec2 boundsMin = ImGui::GetItemRectMin();
	const ImVec2 boundsMax = ImGui::GetItemRectMax();
	const Theme& theme = GetTheme();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// Dear ImGui keeps mouse focus on the last clicked item. The persistent
	// focus ring reads as a selected component, so keep it only for nav input.
	if(ImGui::IsItemFocused() && !ImGui::GetIO().NavActive){
		drawList->AddRect(
			ImVec2(boundsMin.x - 1.5f, boundsMin.y - 1.5f),
			ImVec2(boundsMax.x + 1.5f, boundsMax.y + 1.5f),
			ImGui::GetColorU32(ImGuiCol_ChildBg),
			theme.cornerRadius + 1.5f,
			0,
			3.0f
		);
		drawList->AddRect(
			boundsMin,
			boundsMax,
			ImGui::GetColorU32(EffectiveOutline()),
			theme.cornerRadius,
			0,
			EffectiveStrokeWidth()
		);
	}

	const float iconSize = 15.0f;
	const float centerY = (boundsMin.y + boundsMax.y) * 0.5f;
	DrawEditorIcon(
		icon,
		ImVec2(boundsMin.x + 25.0f, centerY - iconSize * 0.5f),
		iconSize,
		ImGui::IsItemHovered() ? 1.0f : 0.86f
	);
	return pressed;
}

inline TreeRowResult IconTreeRow(
	const char* id,
	const char* visibleLabel,
	const EditorIconImage& icon,
	bool selected,
	bool hasChildren,
	bool open,
	const char* badge = nullptr,
	float width = -1.0f
){
	if(!visibleLabel) visibleLabel = "";
	std::string label = "   ";
	label += visibleLabel;

	TreeRowResult result = TreeRow(
		id,
		label.c_str(),
		selected,
		hasChildren,
		open,
		badge,
		width
	);

	const ImVec2 boundsMin = ImGui::GetItemRectMin();
	const ImVec2 boundsMax = ImGui::GetItemRectMax();
	const float iconSize = 14.0f;
	const float centerY = (boundsMin.y + boundsMax.y) * 0.5f;
	const float iconX = hasChildren
		? boundsMin.x + 25.0f
		: boundsMin.x + 8.0f;
	DrawEditorIcon(
		icon,
		ImVec2(iconX, centerY - iconSize * 0.5f),
		iconSize,
		selected || ImGui::IsItemHovered() ? 1.0f : 0.80f
	);
	return result;
}

} // namespace MImGui
