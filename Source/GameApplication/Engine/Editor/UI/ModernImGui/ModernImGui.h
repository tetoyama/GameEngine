#pragma once

#include <ImGui/imgui.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>

// ModernImGui is intentionally a thin presentation layer over Dear ImGui.
// It keeps the immediate-mode interaction model, docking and backends intact,
// while replacing the visible controls with editor-specific drawing.
namespace MImGui {

struct Theme {
	ImVec4 window          = ImVec4(0.055f, 0.060f, 0.070f, 1.000f);
	ImVec4 panel           = ImVec4(0.070f, 0.076f, 0.088f, 1.000f);
	ImVec4 raised          = ImVec4(0.100f, 0.108f, 0.125f, 1.000f);
	ImVec4 hover           = ImVec4(0.145f, 0.155f, 0.180f, 1.000f);
	ImVec4 pressed         = ImVec4(0.175f, 0.188f, 0.220f, 1.000f);
	ImVec4 selected        = ImVec4(0.125f, 0.245f, 0.435f, 1.000f);
	ImVec4 accent          = ImVec4(0.220f, 0.510f, 0.960f, 1.000f);
	ImVec4 accentHover     = ImVec4(0.285f, 0.575f, 1.000f, 1.000f);
	ImVec4 danger          = ImVec4(0.820f, 0.255f, 0.285f, 1.000f);
	ImVec4 dangerHover     = ImVec4(0.930f, 0.315f, 0.345f, 1.000f);
	ImVec4 textPrimary     = ImVec4(0.925f, 0.935f, 0.950f, 1.000f);
	ImVec4 textSecondary   = ImVec4(0.640f, 0.665f, 0.710f, 1.000f);
	ImVec4 textDisabled    = ImVec4(0.390f, 0.410f, 0.450f, 1.000f);
	ImVec4 separator       = ImVec4(1.000f, 1.000f, 1.000f, 0.075f);
	ImVec4 outline         = ImVec4(1.000f, 1.000f, 1.000f, 0.105f);

	float controlHeight    = 30.0f;
	float compactHeight    = 26.0f;
	float cornerRadius     = 7.0f;
	float panelRadius      = 10.0f;
	float horizontalPad    = 10.0f;
};

inline Theme& GetTheme() {
	static Theme theme;
	return theme;
}

inline ImVec4 Lerp(const ImVec4& from, const ImVec4& to, float amount) {
	amount = std::clamp(amount, 0.0f, 1.0f);
	return ImVec4(
		from.x + (to.x - from.x) * amount,
		from.y + (to.y - from.y) * amount,
		from.z + (to.z - from.z) * amount,
		from.w + (to.w - from.w) * amount
	);
}

inline void ApplyTheme() {
	Theme& theme = GetTheme();
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	colors[ImGuiCol_Text]                  = theme.textPrimary;
	colors[ImGuiCol_TextDisabled]          = theme.textDisabled;
	colors[ImGuiCol_WindowBg]              = theme.window;
	colors[ImGuiCol_ChildBg]               = theme.panel;
	colors[ImGuiCol_PopupBg]               = ImVec4(theme.raised.x, theme.raised.y, theme.raised.z, 0.985f);
	colors[ImGuiCol_Border]                = theme.outline;
	colors[ImGuiCol_BorderShadow]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_FrameBg]               = theme.raised;
	colors[ImGuiCol_FrameBgHovered]        = theme.hover;
	colors[ImGuiCol_FrameBgActive]         = theme.pressed;
	colors[ImGuiCol_TitleBg]               = theme.window;
	colors[ImGuiCol_TitleBgActive]         = theme.panel;
	colors[ImGuiCol_TitleBgCollapsed]      = theme.window;
	colors[ImGuiCol_MenuBarBg]             = theme.window;
	colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.42f, 0.44f, 0.48f, 0.42f);
	colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.52f, 0.54f, 0.58f, 0.64f);
	colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.62f, 0.64f, 0.68f, 0.80f);
	colors[ImGuiCol_CheckMark]             = theme.accent;
	colors[ImGuiCol_SliderGrab]            = theme.accent;
	colors[ImGuiCol_SliderGrabActive]      = theme.accentHover;
	colors[ImGuiCol_Button]                = theme.raised;
	colors[ImGuiCol_ButtonHovered]         = theme.hover;
	colors[ImGuiCol_ButtonActive]          = theme.pressed;
	colors[ImGuiCol_Header]                = ImVec4(theme.selected.x, theme.selected.y, theme.selected.z, 0.56f);
	colors[ImGuiCol_HeaderHovered]         = ImVec4(theme.selected.x, theme.selected.y, theme.selected.z, 0.78f);
	colors[ImGuiCol_HeaderActive]          = theme.selected;
	colors[ImGuiCol_Separator]             = theme.separator;
	colors[ImGuiCol_SeparatorHovered]      = ImVec4(theme.accent.x, theme.accent.y, theme.accent.z, 0.55f);
	colors[ImGuiCol_SeparatorActive]       = theme.accent;
	colors[ImGuiCol_ResizeGrip]            = ImVec4(theme.accent.x, theme.accent.y, theme.accent.z, 0.00f);
	colors[ImGuiCol_ResizeGripHovered]     = ImVec4(theme.accent.x, theme.accent.y, theme.accent.z, 0.32f);
	colors[ImGuiCol_ResizeGripActive]      = ImVec4(theme.accent.x, theme.accent.y, theme.accent.z, 0.60f);
	colors[ImGuiCol_Tab]                   = theme.window;
	colors[ImGuiCol_TabHovered]            = theme.hover;
	colors[ImGuiCol_TabActive]             = theme.panel;
	colors[ImGuiCol_TabUnfocused]          = theme.window;
	colors[ImGuiCol_TabUnfocusedActive]    = theme.panel;
	colors[ImGuiCol_TextSelectedBg]        = ImVec4(theme.accent.x, theme.accent.y, theme.accent.z, 0.32f);
	colors[ImGuiCol_DragDropTarget]        = theme.accentHover;
	colors[ImGuiCol_NavHighlight]          = theme.accent;
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.55f);
	colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.0f, 0.0f, 0.0f, 0.22f);
	colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.0f, 0.0f, 0.0f, 0.50f);

	style.WindowPadding       = ImVec2(10.0f, 9.0f);
	style.FramePadding        = ImVec2(10.0f, 6.0f);
	style.CellPadding         = ImVec2(8.0f, 6.0f);
	style.ItemSpacing         = ImVec2(8.0f, 7.0f);
	style.ItemInnerSpacing    = ImVec2(7.0f, 5.0f);
	style.TouchExtraPadding   = ImVec2(0.0f, 0.0f);
	style.IndentSpacing       = 18.0f;
	style.ScrollbarSize       = 10.0f;
	style.GrabMinSize         = 18.0f;
	style.WindowBorderSize    = 0.0f;
	style.ChildBorderSize     = 0.0f;
	style.PopupBorderSize     = 1.0f;
	style.FrameBorderSize     = 0.0f;
	style.TabBorderSize       = 0.0f;
	style.WindowRounding      = theme.panelRadius;
	style.ChildRounding       = theme.panelRadius;
	style.FrameRounding       = theme.cornerRadius;
	style.PopupRounding       = theme.panelRadius;
	style.ScrollbarRounding   = 10.0f;
	style.GrabRounding        = 10.0f;
	style.TabRounding         = theme.cornerRadius;
	style.LogSliderDeadzone   = 4.0f;
}

struct MotionState {
	float value = 0.0f;
	float velocity = 0.0f;
	int lastVisibleFrame = 0;
};

inline std::unordered_map<ImGuiID, MotionState>& MotionStates() {
	static std::unordered_map<ImGuiID, MotionState> states;
	return states;
}

inline void BeginFrame() {
	static int lastCleanupFrame = 0;
	const int frame = ImGui::GetFrameCount();
	if(frame - lastCleanupFrame < 240) return;

	auto& states = MotionStates();
	for(auto it = states.begin(); it != states.end();){
		if(frame - it->second.lastVisibleFrame > 480){
			it = states.erase(it);
		} else{
			++it;
		}
	}
	lastCleanupFrame = frame;
}

inline float Animate(ImGuiID id, float target, float frequency = 18.0f, float dampingRatio = 1.0f) {
	MotionState& state = MotionStates()[id];
	state.lastVisibleFrame = ImGui::GetFrameCount();

	const float dt = (std::min)(ImGui::GetIO().DeltaTime, 1.0f / 15.0f);
	const float acceleration =
		(target - state.value) * frequency * frequency -
		(2.0f * dampingRatio * frequency) * state.velocity;

	state.velocity += acceleration * dt;
	state.value += state.velocity * dt;

	if(std::fabs(target - state.value) < 0.0005f && std::fabs(state.velocity) < 0.0005f){
		state.value = target;
		state.velocity = 0.0f;
	}
	return state.value;
}

enum class ButtonKind {
	Secondary,
	Primary,
	Danger,
	Ghost
};

inline const char* VisibleLabelEnd(const char* label) {
	if(!label) return label;
	const char* cursor = label;
	while(*cursor){
		if(cursor[0] == '#' && cursor[1] == '#') return cursor;
		++cursor;
	}
	return cursor;
}

inline bool Button(const char* label, const ImVec2& requestedSize = ImVec2(0.0f, 0.0f), ButtonKind kind = ButtonKind::Secondary) {
	Theme& theme = GetTheme();
	const char* textEnd = VisibleLabelEnd(label);
	const ImVec2 textSize = ImGui::CalcTextSize(label, textEnd);
	ImVec2 size = requestedSize;
	if(size.x < 0.0f) size.x = ImGui::GetContentRegionAvail().x;
	else if(size.x == 0.0f) size.x = textSize.x + theme.horizontalPad * 2.0f;
	if(size.y <= 0.0f) size.y = theme.controlHeight;

	const bool pressed = ImGui::InvisibleButton(label, size);
	const ImGuiID id = ImGui::GetItemID();
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive();

	const float hoverAmount = Animate(id ^ 0x4F1BBCDCu, hovered ? 1.0f : 0.0f);
	const float pressAmount = Animate(id ^ 0xA8C26D91u, held ? 1.0f : 0.0f, 24.0f, 0.92f);

	ImVec4 idle = theme.raised;
	ImVec4 hover = theme.hover;
	if(kind == ButtonKind::Primary){
		idle = theme.accent;
		hover = theme.accentHover;
	} else if(kind == ButtonKind::Danger){
		idle = theme.danger;
		hover = theme.dangerHover;
	} else if(kind == ButtonKind::Ghost){
		idle = ImVec4(theme.raised.x, theme.raised.y, theme.raised.z, 0.0f);
		hover = ImVec4(theme.hover.x, theme.hover.y, theme.hover.z, 0.88f);
	}

	ImVec4 fill = Lerp(idle, hover, hoverAmount);
	fill = Lerp(fill, theme.pressed, pressAmount * (kind == ButtonKind::Secondary ? 0.75f : 0.30f));

	ImVec2 min = ImGui::GetItemRectMin();
	ImVec2 max = ImGui::GetItemRectMax();
	const float inset = 1.2f * pressAmount;
	min.x += inset;
	min.y += inset;
	max.x -= inset;
	max.y -= inset;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(min, max, ImGui::GetColorU32(fill), theme.cornerRadius);
	if(kind == ButtonKind::Secondary){
		drawList->AddRect(min, max, ImGui::GetColorU32(theme.outline), theme.cornerRadius);
	}

	const ImVec2 textPosition(
		min.x + ((max.x - min.x) - textSize.x) * 0.5f,
		min.y + ((max.y - min.y) - textSize.y) * 0.5f
	);
	drawList->AddText(textPosition, ImGui::GetColorU32(theme.textPrimary), label, textEnd);
	return pressed;
}

inline bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f)) {
	return Button(label, size, ButtonKind::Primary);
}

inline bool DangerButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f)) {
	return Button(label, size, ButtonKind::Danger);
}

inline bool Toggle(const char* label, bool* value) {
	if(!value) return false;

	Theme& theme = GetTheme();
	const char* textEnd = VisibleLabelEnd(label);
	const float trackWidth = 36.0f;
	const float trackHeight = 20.0f;
	const float spacing = 8.0f;
	const ImVec2 textSize = ImGui::CalcTextSize(label, textEnd);
	const ImVec2 size(trackWidth + spacing + textSize.x, (std::max)(trackHeight, textSize.y));

	const bool pressed = ImGui::InvisibleButton(label, size);
	const ImGuiID id = ImGui::GetItemID();
	if(pressed) *value = !*value;

	const bool hovered = ImGui::IsItemHovered();
	const float enabledAmount = Animate(id ^ 0x6C5C3E79u, *value ? 1.0f : 0.0f, 20.0f, 0.95f);
	const float hoverAmount = Animate(id ^ 0x3B98A6F1u, hovered ? 1.0f : 0.0f);

	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 trackMin(min.x, min.y + (size.y - trackHeight) * 0.5f);
	const ImVec2 trackMax(trackMin.x + trackWidth, trackMin.y + trackHeight);
	ImVec4 trackColor = Lerp(theme.raised, theme.accent, enabledAmount);
	trackColor = Lerp(trackColor, theme.accentHover, hoverAmount * enabledAmount * 0.35f);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(trackMin, trackMax, ImGui::GetColorU32(trackColor), trackHeight * 0.5f);

	const float knobRadius = 7.0f;
	const float knobX = trackMin.x + knobRadius + 3.0f + enabledAmount * (trackWidth - (knobRadius + 3.0f) * 2.0f);
	const float knobY = trackMin.y + trackHeight * 0.5f;
	drawList->AddCircleFilled(ImVec2(knobX, knobY), knobRadius, ImGui::GetColorU32(theme.textPrimary));

	const ImVec2 textPosition(trackMax.x + spacing, min.y + (size.y - textSize.y) * 0.5f);
	drawList->AddText(textPosition, ImGui::GetColorU32(theme.textPrimary), label, textEnd);
	return pressed;
}

inline bool SearchField(const char* id, const char* hint, char* buffer, std::size_t bufferSize, float width = -1.0f) {
	Theme& theme = GetTheme();
	ImGui::PushStyleColor(ImGuiCol_FrameBg, theme.raised);
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, theme.hover);
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, theme.pressed);
	ImGui::PushStyleColor(ImGuiCol_Border, theme.outline);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, theme.cornerRadius);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(theme.horizontalPad, 6.0f));
	ImGui::SetNextItemWidth(width);
	const bool changed = ImGui::InputTextWithHint(id, hint, buffer, bufferSize);
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(4);
	return changed;
}

inline void DrawDisclosureArrow(ImDrawList* drawList, const ImVec2& center, float amount, ImU32 color) {
	const float arrowSize = 4.0f;
	const float angle = amount * 1.57079632679f;
	const float cosine = std::cos(angle);
	const float sine = std::sin(angle);
	ImVec2 points[3] = {
		ImVec2(-arrowSize, -arrowSize),
		ImVec2(-arrowSize,  arrowSize),
		ImVec2( arrowSize,  0.0f)
	};
	for(ImVec2& point : points){
		const float x = point.x * cosine - point.y * sine;
		const float y = point.x * sine + point.y * cosine;
		point = ImVec2(center.x + x, center.y + y);
	}
	drawList->AddTriangleFilled(points[0], points[1], points[2], color);
}

inline bool SectionHeader(const char* label, bool* open, float width = -1.0f) {
	if(!open) return false;
	if(width <= 0.0f) width = ImGui::GetContentRegionAvail().x;

	Theme& theme = GetTheme();
	const char* textEnd = VisibleLabelEnd(label);
	const ImVec2 size(width, theme.controlHeight);
	const bool pressed = ImGui::InvisibleButton(label, size);
	const ImGuiID id = ImGui::GetItemID();
	if(pressed) *open = !*open;

	const bool hovered = ImGui::IsItemHovered();
	const float hoverAmount = Animate(id ^ 0x0E349ACBu, hovered ? 1.0f : 0.0f);
	const float openAmount = Animate(id ^ 0x5B7744C3u, *open ? 1.0f : 0.0f);
	const ImVec4 fill = Lerp(theme.panel, theme.hover, hoverAmount);

	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(min, max, ImGui::GetColorU32(fill), theme.cornerRadius);

	const float centerY = (min.y + max.y) * 0.5f;
	DrawDisclosureArrow(
		drawList,
		ImVec2(min.x + 11.0f, centerY),
		openAmount,
		ImGui::GetColorU32(theme.textSecondary)
	);

	const ImVec2 textSize = ImGui::CalcTextSize(label, textEnd);
	drawList->AddText(
		ImVec2(min.x + 24.0f, centerY - textSize.y * 0.5f),
		ImGui::GetColorU32(theme.textPrimary),
		label,
		textEnd
	);
	return pressed;
}

struct TreeRowResult {
	bool open = false;
	bool activated = false;
	bool arrowActivated = false;
};

inline TreeRowResult TreeRow(
	const char* id,
	const char* label,
	bool selected,
	bool hasChildren,
	bool open,
	const char* badge = nullptr,
	float width = -1.0f
) {
	Theme& theme = GetTheme();
	if(width <= 0.0f) width = ImGui::GetContentRegionAvail().x;
	const ImVec2 size(width, theme.compactHeight);

	const bool pressed = ImGui::InvisibleButton(id, size);
	const ImGuiID itemID = ImGui::GetItemID();
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive();
	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	const ImVec2 mouse = ImGui::GetMousePos();
	const bool arrowHovered = hasChildren && hovered && mouse.x <= min.x + 24.0f;
	const bool doubleClicked = hasChildren && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

	TreeRowResult result;
	result.open = open;
	result.arrowActivated = (pressed && arrowHovered) || doubleClicked;
	if(result.arrowActivated) result.open = !result.open;
	result.activated = pressed && !arrowHovered;

	const float hoverAmount = Animate(itemID ^ 0x8DE42117u, hovered ? 1.0f : 0.0f);
	const float selectedAmount = Animate(itemID ^ 0xD24A3A65u, selected ? 1.0f : 0.0f);
	const float pressAmount = Animate(itemID ^ 0x771BCF09u, held ? 1.0f : 0.0f, 24.0f, 0.92f);
	const float openAmount = Animate(itemID ^ 0x40B6C0D3u, result.open ? 1.0f : 0.0f);

	ImVec4 transparentPanel(theme.panel.x, theme.panel.y, theme.panel.z, 0.0f);
	ImVec4 hoverFill(theme.hover.x, theme.hover.y, theme.hover.z, 0.82f);
	ImVec4 fill = Lerp(transparentPanel, hoverFill, hoverAmount);
	fill = Lerp(fill, theme.selected, selectedAmount);
	fill = Lerp(fill, theme.pressed, pressAmount * 0.30f);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if(fill.w > 0.001f){
		drawList->AddRectFilled(min, max, ImGui::GetColorU32(fill), theme.cornerRadius);
	}

	const float centerY = (min.y + max.y) * 0.5f;
	float textX = min.x + 8.0f;
	if(hasChildren){
		DrawDisclosureArrow(
			drawList,
			ImVec2(min.x + 12.0f, centerY),
			openAmount,
			ImGui::GetColorU32(arrowHovered ? theme.textPrimary : theme.textSecondary)
		);
		textX = min.x + 24.0f;
	}

	float textRight = max.x - 8.0f;
	if(badge && badge[0]){
		const ImVec2 badgeTextSize = ImGui::CalcTextSize(badge);
		const float badgeWidth = badgeTextSize.x + 12.0f;
		const ImVec2 badgeMin(max.x - badgeWidth - 6.0f, centerY - 9.0f);
		const ImVec2 badgeMax(max.x - 6.0f, centerY + 9.0f);
		drawList->AddRectFilled(
			badgeMin,
			badgeMax,
			ImGui::GetColorU32(ImVec4(theme.accent.x, theme.accent.y, theme.accent.z, 0.22f)),
			9.0f
		);
		drawList->AddText(
			ImVec2(badgeMin.x + 6.0f, centerY - badgeTextSize.y * 0.5f),
			ImGui::GetColorU32(theme.accentHover),
			badge
		);
		textRight = badgeMin.x - 6.0f;
	}

	const char* textEnd = VisibleLabelEnd(label);
	const ImVec2 textSize = ImGui::CalcTextSize(label, textEnd);
	drawList->PushClipRect(ImVec2(textX, min.y), ImVec2(textRight, max.y), true);
	drawList->AddText(
		ImVec2(textX, centerY - textSize.y * 0.5f),
		ImGui::GetColorU32(theme.textPrimary),
		label,
		textEnd
	);
	drawList->PopClipRect();
	return result;
}

} // namespace MImGui
