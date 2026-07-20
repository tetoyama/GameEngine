// =======================================================================
//
// EditorIconWidgets.h
// Icon-aware wrappers layered on top of ModernImGui controls.
//
// =======================================================================
#pragma once

#include <algorithm>
#include <string>

#include "EditorIconLibrary.h"
#include "ModernImGui.h"

namespace MImGui {

inline void DrawEditorVectorGlyph(
	EditorIcon icon,
	const ImVec2& topLeft,
	float size,
	float alpha
){
	if(icon == EditorIcon::Count || size <= 0.0f) return;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float opacity = (std::max)(0.0f, (std::min)(1.0f, alpha));
	ImVec4 tint = GetTheme().textPrimary;
	tint.w *= opacity;
	const ImU32 color = ImGui::GetColorU32(tint);
	const float stroke = (std::max)(1.20f, size * 0.105f);
	const float thinStroke = (std::max)(1.0f, stroke * 0.82f);
	const float rounding = size * 0.12f;

	auto point = [&](float x, float y){
		return ImVec2(topLeft.x + x * size, topLeft.y + y * size);
	};
	auto line = [&](float x0, float y0, float x1, float y1, float width = -1.0f){
		drawList->AddLine(
			point(x0, y0),
			point(x1, y1),
			color,
			width > 0.0f ? width : stroke
		);
	};
	auto node = [&](float x, float y, float radius = 0.085f){
		drawList->AddCircleFilled(point(x, y), size * radius, color, 12);
	};

	switch(icon){
		case EditorIcon::Add:
			line(0.22f, 0.50f, 0.78f, 0.50f);
			line(0.50f, 0.22f, 0.50f, 0.78f);
			break;

		case EditorIcon::Assets:
			drawList->AddRect(
				point(0.10f, 0.34f),
				point(0.90f, 0.82f),
				color,
				rounding,
				0,
				stroke
			);
			line(0.14f, 0.34f, 0.14f, 0.25f);
			line(0.14f, 0.25f, 0.42f, 0.25f);
			line(0.42f, 0.25f, 0.53f, 0.34f);
			break;

		case EditorIcon::Collider:
			line(0.50f, 0.10f, 0.84f, 0.30f);
			line(0.84f, 0.30f, 0.50f, 0.50f);
			line(0.50f, 0.50f, 0.16f, 0.30f);
			line(0.16f, 0.30f, 0.50f, 0.10f);
			line(0.16f, 0.30f, 0.16f, 0.70f);
			line(0.84f, 0.30f, 0.84f, 0.70f);
			line(0.50f, 0.50f, 0.50f, 0.90f);
			line(0.16f, 0.70f, 0.50f, 0.90f);
			line(0.84f, 0.70f, 0.50f, 0.90f);
			break;

		case EditorIcon::Component:
			for(int row = 0; row < 2; ++row){
				for(int column = 0; column < 2; ++column){
					const float x = 0.15f + static_cast<float>(column) * 0.39f;
					const float y = 0.15f + static_cast<float>(row) * 0.39f;
					drawList->AddRect(
						point(x, y),
						point(x + 0.30f, y + 0.30f),
						color,
						rounding * 0.72f,
						0,
						stroke
					);
				}
			}
			break;

		case EditorIcon::Console:
			drawList->AddRect(
				point(0.09f, 0.16f),
				point(0.91f, 0.84f),
				color,
				rounding,
				0,
				stroke
			);
			line(0.25f, 0.37f, 0.40f, 0.50f);
			line(0.40f, 0.50f, 0.25f, 0.63f);
			line(0.51f, 0.65f, 0.72f, 0.65f, thinStroke);
			break;

		case EditorIcon::Entity:
			drawList->AddRect(
				point(0.13f, 0.13f),
				point(0.87f, 0.87f),
				color,
				rounding * 1.25f,
				0,
				stroke
			);
			drawList->AddCircle(point(0.50f, 0.50f), size * 0.18f, color, 20, stroke);
			break;

		case EditorIcon::Hierarchy:
			node(0.24f, 0.23f);
			node(0.60f, 0.50f);
			node(0.60f, 0.78f);
			line(0.24f, 0.31f, 0.24f, 0.78f, thinStroke);
			line(0.24f, 0.50f, 0.51f, 0.50f, thinStroke);
			line(0.24f, 0.78f, 0.51f, 0.78f, thinStroke);
			break;

		case EditorIcon::Inspector:
			line(0.14f, 0.27f, 0.86f, 0.27f, thinStroke);
			line(0.14f, 0.50f, 0.86f, 0.50f, thinStroke);
			line(0.14f, 0.73f, 0.86f, 0.73f, thinStroke);
			node(0.35f, 0.27f, 0.075f);
			node(0.67f, 0.50f, 0.075f);
			node(0.45f, 0.73f, 0.075f);
			break;

		case EditorIcon::Layers:
			line(0.50f, 0.13f, 0.88f, 0.34f);
			line(0.88f, 0.34f, 0.50f, 0.55f);
			line(0.50f, 0.55f, 0.12f, 0.34f);
			line(0.12f, 0.34f, 0.50f, 0.13f);
			line(0.16f, 0.52f, 0.50f, 0.71f, thinStroke);
			line(0.50f, 0.71f, 0.84f, 0.52f, thinStroke);
			line(0.16f, 0.68f, 0.50f, 0.87f, thinStroke);
			line(0.50f, 0.87f, 0.84f, 0.68f, thinStroke);
			break;

		case EditorIcon::Name:
			line(0.19f, 0.22f, 0.81f, 0.22f);
			line(0.50f, 0.22f, 0.50f, 0.79f);
			line(0.35f, 0.79f, 0.65f, 0.79f);
			break;

		case EditorIcon::Performance:
			line(0.13f, 0.82f, 0.13f, 0.18f, thinStroke);
			line(0.13f, 0.82f, 0.88f, 0.82f, thinStroke);
			line(0.18f, 0.67f, 0.36f, 0.48f);
			line(0.36f, 0.48f, 0.52f, 0.61f);
			line(0.52f, 0.61f, 0.69f, 0.28f);
			line(0.69f, 0.28f, 0.86f, 0.39f);
			break;

		case EditorIcon::Transform:
			line(0.50f, 0.18f, 0.50f, 0.82f, thinStroke);
			line(0.18f, 0.50f, 0.82f, 0.50f, thinStroke);
			drawList->AddTriangleFilled(point(0.50f, 0.09f), point(0.42f, 0.23f), point(0.58f, 0.23f), color);
			drawList->AddTriangleFilled(point(0.50f, 0.91f), point(0.42f, 0.77f), point(0.58f, 0.77f), color);
			drawList->AddTriangleFilled(point(0.09f, 0.50f), point(0.23f, 0.42f), point(0.23f, 0.58f), color);
			drawList->AddTriangleFilled(point(0.91f, 0.50f), point(0.77f, 0.42f), point(0.77f, 0.58f), color);
			drawList->AddCircleFilled(point(0.50f, 0.50f), size * 0.065f, color, 12);
			break;

		case EditorIcon::Count:
		default:
			break;
	}
}

inline void DrawEditorIcon(
	const EditorIconImage& icon,
	const ImVec2& topLeft,
	float size,
	float alpha = 1.0f
){
	if(!icon.IsValid() || size <= 0.0f) return;
	if(icon.IsVector()){
		DrawEditorVectorGlyph(icon.vectorIcon, topLeft, size, alpha);
		return;
	}
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

	ImVec4 fill = Lerp(WithAlpha(theme.panel, 0.0f), WithAlpha(theme.hover, 0.52f), hoverAmount);
	fill = Lerp(fill, WithAlpha(theme.accent, 0.12f), selectedAmount);
	fill = Lerp(fill, WithAlpha(theme.pressed, 0.70f), pressAmount * 0.30f);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if(fill.w > 0.001f){
		drawList->AddRectFilled(boundsMin, boundsMax, ImGui::GetColorU32(fill), theme.cornerRadius);
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

	if(tooltip && tooltip[0] && hovered) ImGui::SetTooltip("%s", tooltip);
	return pressed;
}

inline float PanelShortcutWidth(const char* visibleLabel, float iconSize = 14.0f){
	if(!visibleLabel) visibleLabel = "";
	return 9.0f + iconSize + 6.0f + ImGui::CalcTextSize(visibleLabel).x + 9.0f;
}

inline bool PanelShortcutButton(
	const char* id,
	const char* visibleLabel,
	const EditorIconImage& icon,
	bool selected,
	const char* tooltip = nullptr,
	float height = 22.0f
){
	if(!id) id = "PanelShortcut";
	if(!visibleLabel) visibleLabel = "";

	const float iconSize = 14.0f;
	const float width = PanelShortcutWidth(visibleLabel, iconSize);
	const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, height));
	const ImGuiID itemID = ImGui::GetItemID();
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive();
	const bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavActive;

	const float hoverAmount = Animate(itemID ^ 0x6A12D459u, hovered ? 1.0f : 0.0f);
	const float selectedAmount = Animate(itemID ^ 0x2C84B173u, selected ? 1.0f : 0.0f);
	const float pressAmount = AnimateInteractive(itemID ^ 0x4178EA25u, held, 0.62f, 25.0f, 1.0f);

	const ImVec2 boundsMin = ImGui::GetItemRectMin();
	const ImVec2 boundsMax = ImGui::GetItemRectMax();
	const float centerY = (boundsMin.y + boundsMax.y) * 0.5f;
	Theme& theme = GetTheme();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	ImVec4 fill = Lerp(WithAlpha(theme.panel, 0.0f), WithAlpha(theme.hover, 0.46f), hoverAmount);
	fill = Lerp(fill, WithAlpha(theme.accent, 0.08f), selectedAmount);
	fill = Lerp(fill, WithAlpha(theme.pressed, 0.64f), pressAmount * 0.28f);
	if(fill.w > 0.001f){
		drawList->AddRectFilled(boundsMin, boundsMax, ImGui::GetColorU32(fill), theme.cornerRadius);
	}
	if(selectedAmount > 0.001f){
		drawList->AddRectFilled(
			ImVec2(boundsMin.x + 7.0f, boundsMax.y - 2.0f),
			ImVec2(boundsMax.x - 7.0f, boundsMax.y - 1.0f),
			ImGui::GetColorU32(WithAlpha(theme.accentHover, 0.90f * selectedAmount)),
			1.0f
		);
	}
	if(focused) DrawFocusRing(drawList, boundsMin, boundsMax, theme.cornerRadius);

	const float iconX = boundsMin.x + 9.0f;
	DrawEditorIcon(icon, ImVec2(iconX, centerY - iconSize * 0.5f), iconSize, hovered || selected ? 1.0f : 0.72f);

	const ImVec4 textColor = hovered || selected ? theme.textPrimary : theme.textSecondary;
	const ImVec2 textSize = ImGui::CalcTextSize(visibleLabel);
	drawList->AddText(
		ImVec2(iconX + iconSize + 6.0f, centerY - textSize.y * 0.5f),
		ImGui::GetColorU32(textColor),
		visibleLabel
	);

	if(tooltip && tooltip[0] && hovered) ImGui::SetTooltip("%s", tooltip);
	return pressed;
}

inline bool IconButton(
	const char* id,
	const char* visibleLabel,
	const EditorIconImage& icon,
	const ImVec2& requestedSize = ImVec2(0.0f, 0.0f),
	ButtonKind kind = ButtonKind::Secondary,
	float iconSize = 16.0f
){
	if(!id) id = "IconButton";
	if(!visibleLabel) visibleLabel = "";

	const Theme& theme = GetTheme();
	const ImVec2 textSize = ImGui::CalcTextSize(visibleLabel);
	ImVec2 size = requestedSize;
	if(size.x == 0.0f) size.x = theme.horizontalPad * 2.0f + iconSize + 7.0f + textSize.x;
	if(size.y <= 0.0f) size.y = theme.controlHeight;

	std::string hiddenLabel = "##";
	hiddenLabel += id;
	const bool pressed = Button(hiddenLabel.c_str(), size, kind);
	const ImVec2 boundsMin = ImGui::GetItemRectMin();
	const ImVec2 boundsMax = ImGui::GetItemRectMax();
	const float centerY = (boundsMin.y + boundsMax.y) * 0.5f;
	const float contentWidth = iconSize + 7.0f + textSize.x;
	const float contentX = boundsMin.x + ((boundsMax.x - boundsMin.x) - contentWidth) * 0.5f;
	const float alpha = ImGui::IsItemHovered() || ImGui::IsItemActive() ? 1.0f : 0.84f;

	DrawEditorIcon(icon, ImVec2(contentX, centerY - iconSize * 0.5f), iconSize, alpha);
	ImGui::GetWindowDrawList()->AddText(
		ImVec2(contentX + iconSize + 7.0f, centerY - textSize.y * 0.5f),
		ImGui::GetColorU32(theme.textPrimary),
		visibleLabel
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
	if(!open) return false;
	if(!id) id = "IconSection";
	if(!visibleLabel) visibleLabel = "";
	if(width <= 0.0f) width = ImGui::GetContentRegionAvail().x;

	Theme& theme = GetTheme();
	const ImVec2 size(width, theme.controlHeight);
	const bool pressed = ImGui::InvisibleButton(id, size);
	const ImGuiID itemID = ImGui::GetItemID();
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive();
	const bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavActive;
	if(pressed) *open = !*open;

	const float hoverAmount = Animate(itemID ^ 0x0E349ACBu, hovered ? 1.0f : 0.0f);
	const float pressAmount = AnimateInteractive(itemID ^ 0x7295AE43u, held, 0.62f, 25.0f, 1.0f);
	const float openAmount = Animate(itemID ^ 0x5B7744C3u, *open ? 1.0f : 0.0f, 18.0f, 1.0f);
	ImVec4 fill = Lerp(theme.raised, theme.hover, hoverAmount);
	fill = Lerp(fill, theme.pressed, pressAmount * 0.30f);

	ImVec2 boundsMin = ImGui::GetItemRectMin();
	ImVec2 boundsMax = ImGui::GetItemRectMax();
	const float inset = 0.7f * pressAmount;
	boundsMin.x += inset;
	boundsMin.y += inset;
	boundsMax.x -= inset;
	boundsMax.y -= inset;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(
		ImVec2(boundsMin.x, boundsMin.y + 1.0f),
		ImVec2(boundsMax.x, boundsMax.y + 1.0f),
		ImGui::GetColorU32(ImVec4(0, 0, 0, 0.14f)),
		theme.cornerRadius
	);
	drawList->AddRectFilled(boundsMin, boundsMax, ImGui::GetColorU32(fill), theme.cornerRadius);
	drawList->AddRect(boundsMin, boundsMax, ImGui::GetColorU32(EffectiveOutline()), theme.cornerRadius, 0, EffectiveStrokeWidth());
	DrawMaterialEdge(drawList, boundsMin, boundsMax, theme.cornerRadius, 1.0f - pressAmount);
	if(focused) DrawFocusRing(drawList, boundsMin, boundsMax, theme.cornerRadius);

	const float centerY = (boundsMin.y + boundsMax.y) * 0.5f;
	DrawDisclosureArrow(
		drawList,
		ImVec2(boundsMin.x + 12.0f, centerY),
		openAmount,
		ImGui::GetColorU32(hovered ? theme.textPrimary : theme.textSecondary)
	);

	const float iconSize = 14.0f;
	const float iconX = boundsMin.x + 25.0f;
	DrawEditorIcon(icon, ImVec2(iconX, centerY - iconSize * 0.5f), iconSize, hovered ? 1.0f : 0.82f);

	const float textX = iconX + iconSize + 6.0f;
	const ImVec2 textSize = ImGui::CalcTextSize(visibleLabel);
	drawList->PushClipRect(ImVec2(textX, boundsMin.y), ImVec2(boundsMax.x - 6.0f, boundsMax.y), true);
	drawList->AddText(
		ImVec2(textX, centerY - textSize.y * 0.5f),
		ImGui::GetColorU32(theme.textPrimary),
		visibleLabel
	);
	drawList->PopClipRect();
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
	if(!id) id = "IconTreeRow";
	if(!visibleLabel) visibleLabel = "";
	Theme& theme = GetTheme();
	if(width <= 0.0f) width = ImGui::GetContentRegionAvail().x;
	const ImVec2 size(width, theme.compactHeight);

	const bool pressed = ImGui::InvisibleButton(id, size);
	const ImGuiID itemID = ImGui::GetItemID();
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive();
	const bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavActive;
	const ImVec2 boundsMin = ImGui::GetItemRectMin();
	const ImVec2 boundsMax = ImGui::GetItemRectMax();
	const ImVec2 mouse = ImGui::GetMousePos();
	const bool arrowHovered = hasChildren && hovered && mouse.x <= boundsMin.x + 26.0f;
	const bool doubleClicked = hasChildren && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

	TreeRowResult result;
	result.open = open;
	result.arrowActivated = (pressed && arrowHovered) || doubleClicked;
	if(result.arrowActivated) result.open = !result.open;
	result.activated = pressed && !arrowHovered;

	const float hoverAmount = Animate(itemID ^ 0x8DE42117u, hovered ? 1.0f : 0.0f);
	const float selectedAmount = Animate(itemID ^ 0xD24A3A65u, selected ? 1.0f : 0.0f);
	const float pressAmount = AnimateInteractive(itemID ^ 0x771BCF09u, held, 0.60f, 25.0f, 1.0f);
	const float openAmount = Animate(itemID ^ 0x40B6C0D3u, result.open ? 1.0f : 0.0f, 18.0f, 1.0f);

	ImVec4 fill = Lerp(WithAlpha(theme.panel, 0.0f), WithAlpha(theme.hover, 0.76f), hoverAmount);
	fill = Lerp(fill, WithAlpha(theme.selected, 0.66f), selectedAmount);
	fill = Lerp(fill, theme.pressed, pressAmount * 0.26f);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if(fill.w > 0.001f) drawList->AddRectFilled(boundsMin, boundsMax, ImGui::GetColorU32(fill), theme.cornerRadius);
	if(selectedAmount > 0.001f){
		drawList->AddRectFilled(
			ImVec2(boundsMin.x + 1.5f, boundsMin.y + 4.0f),
			ImVec2(boundsMin.x + 4.0f, boundsMax.y - 4.0f),
			ImGui::GetColorU32(WithAlpha(theme.accentHover, 0.95f * selectedAmount)),
			1.25f
		);
	}
	if(focused) DrawFocusRing(drawList, boundsMin, boundsMax, theme.cornerRadius);

	const float centerY = (boundsMin.y + boundsMax.y) * 0.5f;
	if(hasChildren){
		DrawDisclosureArrow(
			drawList,
			ImVec2(boundsMin.x + 13.0f, centerY),
			openAmount,
			ImGui::GetColorU32(arrowHovered ? theme.textPrimary : theme.textSecondary)
		);
	}

	const float iconSize = 14.0f;
	const float iconX = hasChildren ? boundsMin.x + 27.0f : boundsMin.x + 8.0f;
	DrawEditorIcon(icon, ImVec2(iconX, centerY - iconSize * 0.5f), iconSize, selected || hovered ? 1.0f : 0.76f);
	const float textX = iconX + iconSize + 6.0f;

	float textRight = boundsMax.x - 8.0f;
	if(badge && badge[0]){
		const ImVec2 badgeTextSize = ImGui::CalcTextSize(badge);
		const float badgeWidth = badgeTextSize.x + 12.0f;
		const ImVec2 badgeMin(boundsMax.x - badgeWidth - 6.0f, centerY - 9.0f);
		const ImVec2 badgeMax(boundsMax.x - 6.0f, centerY + 9.0f);
		drawList->AddRectFilled(badgeMin, badgeMax, ImGui::GetColorU32(WithAlpha(theme.accent, 0.24f)), 9.0f);
		drawList->AddRect(
			badgeMin,
			badgeMax,
			ImGui::GetColorU32(WithAlpha(theme.accentHover, 0.42f)),
			9.0f,
			0,
			1.0f
		);
		drawList->AddText(
			ImVec2(badgeMin.x + 6.0f, centerY - badgeTextSize.y * 0.5f),
			ImGui::GetColorU32(theme.accentHover),
			badge
		);
		textRight = badgeMin.x - 6.0f;
	}

	const ImVec2 textSize = ImGui::CalcTextSize(visibleLabel);
	drawList->PushClipRect(ImVec2(textX, boundsMin.y), ImVec2(textRight, boundsMax.y), true);
	drawList->AddText(
		ImVec2(textX, centerY - textSize.y * 0.5f),
		ImGui::GetColorU32(theme.textPrimary),
		visibleLabel
	);
	drawList->PopClipRect();
	return result;
}

} // namespace MImGui
