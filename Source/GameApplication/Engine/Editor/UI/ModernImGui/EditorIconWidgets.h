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
