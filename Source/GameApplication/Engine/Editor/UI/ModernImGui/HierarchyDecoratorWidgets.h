// =======================================================================
//
// HierarchyDecoratorWidgets.h
// Hierarchy-specific presentation inspired by HierarchyDecorator while
// retaining the restrained material and motion language of ModernImGui.
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "EditorIconWidgets.h"

namespace MImGui {

struct HierarchyAccessoryGlyph {
	EditorIconImage icon;
	std::string tooltip;
};

struct HierarchySceneHeaderResult {
	bool open = false;
	bool activated = false;
};

struct DecoratedHierarchyRowResult : TreeRowResult {
	bool activeToggled = false;
	bool accessoryHovered = false;
	float textMinX = 0.0f;
	float textMaxX = 0.0f;
};

inline bool HierarchyPointInRect(
	const ImVec2& point,
	const ImVec2& minimum,
	const ImVec2& maximum
){
	return point.x >= minimum.x && point.x <= maximum.x &&
		point.y >= minimum.y && point.y <= maximum.y;
}

inline HierarchySceneHeaderResult HierarchySceneHeader(
	const char* id,
	const char* label,
	const EditorIconImage& icon,
	int entityCount,
	bool open,
	float width = -1.0f
){
	if(!id) id = "HierarchySceneHeader";
	if(!label) label = "Scene";
	if(width <= 0.0f) width = ImGui::GetContentRegionAvail().x;

	Theme& theme = GetTheme();
	const ImVec2 size(width, theme.controlHeight + 2.0f);
	const bool pressed = ImGui::InvisibleButton(id, size);
	const ImGuiID itemID = ImGui::GetItemID();
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive();
	const bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavActive;
	const bool doubleClicked = hovered &&
		ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

	HierarchySceneHeaderResult result;
	result.open = open;
	result.activated = pressed || doubleClicked;
	if(result.activated) result.open = !result.open;

	const float hoverAmount = Animate(
		itemID ^ 0x2F8514C7u,
		hovered ? 1.0f : 0.0f
	);
	const float pressAmount = AnimateInteractive(
		itemID ^ 0x4EA73519u,
		held,
		0.62f,
		25.0f,
		1.0f
	);
	const float openAmount = Animate(
		itemID ^ 0x730C9AF1u,
		result.open ? 1.0f : 0.0f,
		18.0f,
		1.0f
	);

	ImVec2 boundsMin = ImGui::GetItemRectMin();
	ImVec2 boundsMax = ImGui::GetItemRectMax();
	const float inset = 0.6f * pressAmount;
	boundsMin.x += inset;
	boundsMin.y += inset;
	boundsMax.x -= inset;
	boundsMax.y -= inset;

	ImVec4 fill = Lerp(theme.panel, theme.raised, 0.72f);
	fill = Lerp(fill, theme.hover, hoverAmount * 0.58f);
	fill = Lerp(fill, theme.pressed, pressAmount * 0.24f);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(
		ImVec2(boundsMin.x, boundsMin.y + 1.5f),
		ImVec2(boundsMax.x, boundsMax.y + 1.5f),
		ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.15f)),
		theme.cornerRadius
	);
	drawList->AddRectFilled(
		boundsMin,
		boundsMax,
		ImGui::GetColorU32(fill),
		theme.cornerRadius
	);
	drawList->AddRect(
		boundsMin,
		boundsMax,
		ImGui::GetColorU32(EffectiveOutline()),
		theme.cornerRadius,
		0,
		EffectiveStrokeWidth()
	);
	DrawMaterialEdge(
		drawList,
		boundsMin,
		boundsMax,
		theme.cornerRadius,
		1.0f - pressAmount
	);
	if(focused){
		DrawFocusRing(drawList, boundsMin, boundsMax, theme.cornerRadius);
	}

	const float centerY = (boundsMin.y + boundsMax.y) * 0.5f;
	DrawDisclosureArrow(
		drawList,
		ImVec2(boundsMin.x + 13.0f, centerY),
		openAmount,
		ImGui::GetColorU32(
			hovered ? theme.textPrimary : theme.textSecondary
		)
	);

	const float iconSize = 15.0f;
	const float iconX = boundsMin.x + 27.0f;
	DrawEditorIcon(
		icon,
		ImVec2(iconX, centerY - iconSize * 0.5f),
		iconSize,
		hovered ? 1.0f : 0.80f
	);

	char countText[32]{};
	std::snprintf(
		countText,
		sizeof(countText),
		entityCount == 1 ? "1 object" : "%d objects",
		entityCount
	);
	const ImVec2 countSize = ImGui::CalcTextSize(countText);
	const float countWidth = countSize.x + 14.0f;
	const ImVec2 countMin(
		boundsMax.x - countWidth - 7.0f,
		centerY - 9.0f
	);
	const ImVec2 countMax(boundsMax.x - 7.0f, centerY + 9.0f);
	drawList->AddRectFilled(
		countMin,
		countMax,
		ImGui::GetColorU32(WithAlpha(theme.window, 0.72f)),
		9.0f
	);
	drawList->AddText(
		ImVec2(countMin.x + 7.0f, centerY - countSize.y * 0.5f),
		ImGui::GetColorU32(theme.textSecondary),
		countText
	);

	const float textX = iconX + iconSize + 7.0f;
	const ImVec2 textSize = ImGui::CalcTextSize(label);
	drawList->PushClipRect(
		ImVec2(textX, boundsMin.y),
		ImVec2(countMin.x - 8.0f, boundsMax.y),
		true
	);
	drawList->AddText(
		ImVec2(textX, centerY - textSize.y * 0.5f),
		ImGui::GetColorU32(theme.textPrimary),
		label
	);
	drawList->PopClipRect();

	return result;
}

inline DecoratedHierarchyRowResult DecoratedHierarchyRow(
	const char* id,
	const char* label,
	bool selected,
	bool hasChildren,
	bool open,
	bool active,
	bool showActiveState,
	bool isPrefab,
	int depth,
	bool isLastChild,
	const std::vector<bool>& ancestorContinuations,
	const std::vector<HierarchyAccessoryGlyph>& glyphs,
	bool showBreadcrumbs,
	bool alternateBackground,
	float width = -1.0f
){
	if(!id) id = "DecoratedHierarchyRow";
	if(!label) label = "";
	if(width <= 0.0f) width = ImGui::GetContentRegionAvail().x;

	Theme& theme = GetTheme();
	const ImVec2 size(width, theme.compactHeight);
	const bool pressed = ImGui::InvisibleButton(id, size);
	const ImGuiID itemID = ImGui::GetItemID();
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive();
	const bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavActive;
	const ImVec2 boundsMin = ImGui::GetItemRectMin();
	const ImVec2 boundsMax = ImGui::GetItemRectMax();
	const ImVec2 mouse = ImGui::GetMousePos();
	const float centerY = (boundsMin.y + boundsMax.y) * 0.5f;

	// Active state is the first semantic control in the row. The tree starts
	// after its fixed leading region so the dot, breadcrumbs, arrow and label
	// never compete for the same pixels at any hierarchy depth.
	constexpr float indentStep = 18.0f;
	constexpr float activeRegionWidth = 22.0f;
	const float treeBaseX = boundsMin.x + (showActiveState ? 31.0f : 11.0f);
	const float nodeX = treeBaseX + static_cast<float>(depth) * indentStep;
	const ImVec2 arrowMin(nodeX - 9.0f, boundsMin.y);
	const ImVec2 arrowMax(nodeX + 10.0f, boundsMax.y);
	const bool arrowHovered = hasChildren && hovered &&
		HierarchyPointInRect(mouse, arrowMin, arrowMax);

	const ImVec2 activeMin(boundsMin.x, boundsMin.y);
	const ImVec2 activeMax(boundsMin.x + activeRegionWidth, boundsMax.y);
	const bool activeHovered = showActiveState && hovered &&
		HierarchyPointInRect(mouse, activeMin, activeMax);
	const bool doubleClicked = hasChildren && hovered && !activeHovered &&
		ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

	DecoratedHierarchyRowResult result;
	result.open = open;
	result.arrowActivated = (pressed && arrowHovered) || doubleClicked;
	if(result.arrowActivated) result.open = !result.open;
	result.activeToggled = pressed && activeHovered;
	result.activated = pressed && !arrowHovered && !activeHovered;

	const float hoverAmount = Animate(
		itemID ^ 0x4E76C2A1u,
		hovered ? 1.0f : 0.0f
	);
	const float selectedAmount = Animate(
		itemID ^ 0x9B21D475u,
		selected ? 1.0f : 0.0f
	);
	const float pressAmount = AnimateInteractive(
		itemID ^ 0x6F3A88D9u,
		held,
		0.60f,
		25.0f,
		1.0f
	);
	const float openAmount = Animate(
		itemID ^ 0x25D06AB3u,
		result.open ? 1.0f : 0.0f,
		18.0f,
		1.0f
	);
	const float activeHoverAmount = Animate(
		itemID ^ 0x3C911E57u,
		activeHovered ? 1.0f : 0.0f
	);

	ImVec4 fill = alternateBackground
		? WithAlpha(theme.raised, 0.20f)
		: WithAlpha(theme.panel, 0.0f);
	fill = Lerp(fill, WithAlpha(theme.hover, 0.70f), hoverAmount);
	fill = Lerp(fill, WithAlpha(theme.selected, 0.64f), selectedAmount);
	fill = Lerp(fill, theme.pressed, pressAmount * 0.24f);

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
			ImVec2(boundsMin.x + 1.5f, boundsMin.y + 4.0f),
			ImVec2(boundsMin.x + 4.0f, boundsMax.y - 4.0f),
			ImGui::GetColorU32(
				WithAlpha(theme.accentHover, 0.96f * selectedAmount)
			),
			1.25f
		);
	}
	if(focused){
		DrawFocusRing(drawList, boundsMin, boundsMax, theme.cornerRadius);
	}

	if(showActiveState){
		const ImVec2 activeCenter(boundsMin.x + 11.0f, centerY);
		const float activeRadius = 4.4f + activeHoverAmount * 0.5f;
		const ImVec4 activeFill = active
			? Lerp(theme.accent, theme.accentHover, activeHoverAmount)
			: WithAlpha(theme.window, 0.78f);
		drawList->AddCircleFilled(
			activeCenter,
			activeRadius,
			ImGui::GetColorU32(activeFill)
		);
		drawList->AddCircle(
			activeCenter,
			activeRadius,
			ImGui::GetColorU32(
				active ? WithAlpha(theme.accentHover, 0.88f) : EffectiveOutline()
			),
			0,
			1.0f
		);
		if(active){
			drawList->AddCircleFilled(
				activeCenter,
				1.35f,
				ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.90f))
			);
		}
		if(activeHovered){
			result.accessoryHovered = true;
			ImGui::SetTooltip(active ? "Disable entity" : "Enable entity");
		}
	}

	if(showBreadcrumbs){
		const ImU32 breadcrumbColor = ImGui::GetColorU32(
			WithAlpha(theme.textDisabled, 0.34f)
		);
		for(std::size_t index = 0; index < ancestorContinuations.size(); ++index){
			if(!ancestorContinuations[index]) continue;
			const float lineX = treeBaseX +
				static_cast<float>(index) * indentStep;
			drawList->AddLine(
				ImVec2(lineX, boundsMin.y),
				ImVec2(lineX, boundsMax.y),
				breadcrumbColor,
				1.0f
			);
		}

		if(depth > 0){
			const float parentX = treeBaseX +
				static_cast<float>(depth - 1) * indentStep;
			drawList->AddLine(
				ImVec2(parentX, boundsMin.y),
				ImVec2(parentX, isLastChild ? centerY : boundsMax.y),
				breadcrumbColor,
				1.0f
			);
			drawList->AddLine(
				ImVec2(parentX, centerY),
				ImVec2(nodeX, centerY),
				breadcrumbColor,
				1.0f
			);
		}
	}

	if(hasChildren){
		DrawDisclosureArrow(
			drawList,
			ImVec2(nodeX, centerY),
			openAmount,
			ImGui::GetColorU32(
				arrowHovered ? theme.textPrimary : theme.textSecondary
			)
		);
	} else if(depth > 0 && showBreadcrumbs){
		drawList->AddCircleFilled(
			ImVec2(nodeX, centerY),
			1.65f,
			ImGui::GetColorU32(WithAlpha(theme.textDisabled, 0.64f))
		);
	}

	float rightCursor = boundsMax.x - 5.0f;
	const float textX = nodeX + 12.0f;
	const float remainingWidth = rightCursor - textX;
	const bool compactPrefab = remainingWidth < 170.0f;
	if(isPrefab){
		const char* prefabText = compactPrefab ? "P" : "Prefab";
		const ImVec2 prefabTextSize = ImGui::CalcTextSize(prefabText);
		const float prefabWidth = prefabTextSize.x + 12.0f;
		const ImVec2 prefabMin(
			rightCursor - prefabWidth,
			centerY - 8.5f
		);
		const ImVec2 prefabMax(rightCursor, centerY + 8.5f);
		drawList->AddRectFilled(
			prefabMin,
			prefabMax,
			ImGui::GetColorU32(WithAlpha(theme.accent, 0.22f)),
			8.5f
		);
		drawList->AddRect(
			prefabMin,
			prefabMax,
			ImGui::GetColorU32(WithAlpha(theme.accentHover, 0.40f)),
			8.5f,
			0,
			1.0f
		);
		drawList->AddText(
			ImVec2(
				prefabMin.x + 6.0f,
				centerY - prefabTextSize.y * 0.5f
			),
			ImGui::GetColorU32(theme.accentHover),
			prefabText
		);
		if(hovered && HierarchyPointInRect(mouse, prefabMin, prefabMax)){
			result.accessoryHovered = true;
			ImGui::SetTooltip("Prefab instance");
		}
		rightCursor = prefabMin.x - 5.0f;
	}

	const float glyphSize = 14.0f;
	const float glyphStride = 18.0f;
	const float minimumTextWidth = 62.0f;
	const float glyphCapacityWidth = (std::max)(
		0.0f,
		rightCursor - textX - minimumTextWidth
	);
	const int maxGlyphCount = (std::min)(
		3,
		static_cast<int>(glyphCapacityWidth / glyphStride)
	);
	const int visibleGlyphCount = (std::min)(
		maxGlyphCount,
		static_cast<int>(glyphs.size())
	);
	if(visibleGlyphCount > 0){
		const float glyphStart = rightCursor -
			static_cast<float>(visibleGlyphCount) * glyphStride;
		for(int index = 0; index < visibleGlyphCount; ++index){
			const float glyphX = glyphStart +
				static_cast<float>(index) * glyphStride;
			const ImVec2 glyphMin(glyphX, centerY - glyphSize * 0.5f);
			const ImVec2 glyphMax(glyphX + glyphSize, centerY + glyphSize * 0.5f);
			DrawEditorIcon(
				glyphs[static_cast<std::size_t>(index)].icon,
				glyphMin,
				glyphSize,
				active ? (hovered ? 1.0f : 0.76f) : 0.40f
			);
			if(hovered && HierarchyPointInRect(mouse, glyphMin, glyphMax)){
				result.accessoryHovered = true;
				const std::string& tooltip =
					glyphs[static_cast<std::size_t>(index)].tooltip;
				if(!tooltip.empty()) ImGui::SetTooltip("%s", tooltip.c_str());
			}
		}
		rightCursor = glyphStart - 4.0f;
	}

	result.textMinX = textX;
	result.textMaxX = rightCursor;
	const ImVec4 textColor = active
		? theme.textPrimary
		: WithAlpha(theme.textDisabled, 0.82f);
	const ImVec2 textSize = ImGui::CalcTextSize(label);
	drawList->PushClipRect(
		ImVec2(textX, boundsMin.y),
		ImVec2(rightCursor, boundsMax.y),
		true
	);
	drawList->AddText(
		ImVec2(textX, centerY - textSize.y * 0.5f),
		ImGui::GetColorU32(textColor),
		label
	);
	drawList->PopClipRect();

	return result;
}

} // namespace MImGui
