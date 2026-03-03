// =======================================================================
// 
// ImGuiFunc.cpp
// 
// =======================================================================

// -----------------------------------------------------------------------
// インクルード
// -----------------------------------------------------------------------
#include "ImGuiFunc.h"
#include <ImGui/imgui.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <myVector3.h>
#include <ImGui/imgui_internal.h>
#include "Editor/Command/CommandManager.h"
#include "Editor/Command/PropertyChangeCommand.h"

#define IMGUI_LABEL_WIDTH	(120.0f)

// -----------------------------------------------------------------------
// グローバル CommandManager ポインタ
// -----------------------------------------------------------------------
static CommandManager* s_commandManager = nullptr;

void ImGui::SetCommandManager(CommandManager* mgr) {
	s_commandManager = mgr;
}

CommandManager* ImGui::GetCommandManager() {
	return s_commandManager;
}

// -----------------------------------------------------------------------
// ImGui::DragVec3
// 
// ImGuiでVector3をドラッグ可能にする関数
// -----------------------------------------------------------------------
bool ImGui::DragVec3(const char* label, Vector3& Vec3, bool readOnly){
	bool isChanged = false;

	float labelWidth = 120.0f;
	int axisCount = 3;
	float spacing = ImGui::GetStyle().ItemSpacing.x;
	float totalRegion = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX();
	float fieldRegion = totalRegion - labelWidth - spacing * 2 * (axisCount);
	float fieldWidth = fieldRegion / axisCount;

	ImVec4 colorX = ImVec4(0.7f, 0.4f, 0.4f, 0.3f);
	ImVec4 colorY = ImVec4(0.4f, 0.7f, 0.4f, 0.3f);
	ImVec4 colorZ = ImVec4(0.4f, 0.4f, 0.7f, 0.3f);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::SameLine();
	ImGui::SetCursorPosX(labelWidth);

	auto DrawComponent = [&](const char* id, float& value, const ImVec4& borderColor, const char* uniqueId, bool isLast){
		ImGui::PushID(uniqueId);
		ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushItemWidth(fieldWidth - 10.0f);

		ImGui::Text("%s", id);
		ImGui::SameLine(0.0f, 10.0f);

		bool changed = false;
		if(!readOnly){
			changed = ImGui::DragFloat("##", &value, 0.01f, -1000.0f, 1000.0f);
		} else{
			ImGui::Text("%.3f", value); // 読み取り専用の場合は値を表示
		}

		ImGui::PopItemWidth();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		ImGui::PopID();

		if(!isLast) ImGui::SameLine();

		if(changed) isChanged = true;
	};

	DrawComponent("X", Vec3.x, colorX, (std::string(label) + "X").c_str(), false);
	DrawComponent("Y", Vec3.y, colorY, (std::string(label) + "Y").c_str(), false);
	DrawComponent("Z", Vec3.z, colorZ, (std::string(label) + "Z").c_str(), true);

	return isChanged;
}

void ImGui::DrawVerticalText(const char* text){
	ImFont* font = ImGui::GetFont();
	float fontSize = ImGui::GetFontSize();

	ImVec2 start = ImGui::GetCursorScreenPos();
	ImDrawList* draw = ImGui::GetWindowDrawList();

	float yOffset = 0.0f;
	float maxWidth = 0.0f;

	for(const char* p = text; *p; ++p){
		char buf[2] = {*p, 0};

		ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buf);

		draw->AddText(
			font,
			fontSize,
			ImVec2(start.x, start.y + yOffset),
			ImGui::GetColorU32(ImGuiCol_Text),
			buf
		);

		yOffset += size.y;
		maxWidth = ImMax(maxWidth, size.x);
	}

	// レイアウト確保
	ImGui::Dummy(ImVec2(maxWidth, yOffset));
}

// -----------------------------------------------------------------------
// アンドゥ対応ラッパー実装
// -----------------------------------------------------------------------

// ## 始まりのラベルは ImGui 内部用なので除外し、それ以外はそのまま操作名として使う
static std::string MakePropertyDesc(const char* label) {
	if (!label || !label[0]) return "";
	if (label[0] == '#' && label[1] != '\0' && label[1] == '#') return "";
	return std::string(label);
}

bool ImGui::UndoDragFloat(const char* label, float* v, float speed,
	float v_min, float v_max, const char* format)
{
	static std::unordered_map<ImGuiID, float> s_capturedFloats;

	float preValue = *v;
	bool changed = ImGui::DragFloat(label, v, speed, v_min, v_max, format);
	ImGuiID id = ImGui::GetItemID();

	if (ImGui::IsItemActivated()) {
		s_capturedFloats[id] = preValue;
	}
	if (ImGui::IsItemDeactivatedAfterEdit() && s_commandManager) {
		auto it = s_capturedFloats.find(id);
		if (it != s_capturedFloats.end()) {
			float oldValue = it->second;
			float newValue = *v;
			s_commandManager->Execute(
				std::make_unique<PropertyChangeCommand<float>>(v, oldValue, newValue, MakePropertyDesc(label))
			);
			s_capturedFloats.erase(it);
		}
	}

	return changed;
}

bool ImGui::UndoDragFloat2(const char* label, float v[2], float speed,
	float v_min, float v_max, const char* format)
{
	struct Float2 { float x, y; };
	static std::unordered_map<ImGuiID, Float2> s_capturedFloat2s;

	Float2* vf = reinterpret_cast<Float2*>(v);
	Float2 preValue = *vf;

	bool changed = ImGui::DragFloat2(label, v, speed, v_min, v_max, format);
	ImGuiID id = ImGui::GetItemID();

	if (ImGui::IsItemActivated()) {
		s_capturedFloat2s[id] = preValue;
	}
	if (ImGui::IsItemDeactivatedAfterEdit() && s_commandManager) {
		auto it = s_capturedFloat2s.find(id);
		if (it != s_capturedFloat2s.end()) {
			Float2 newValue = *vf;
			s_commandManager->Execute(
				std::make_unique<PropertyChangeCommand<Float2>>(vf, it->second, newValue, MakePropertyDesc(label))
			);
			s_capturedFloat2s.erase(it);
		}
	}

	return changed;
}

bool ImGui::UndoDragFloat3(const char* label, float v[3], float speed,
	float v_min, float v_max, const char* format)
{
	struct Float3 { float x, y, z; };
	static std::unordered_map<ImGuiID, Float3> s_capturedFloat3s;

	Float3* vf = reinterpret_cast<Float3*>(v);
	Float3 preValue = *vf;

	bool changed = ImGui::DragFloat3(label, v, speed, v_min, v_max, format);
	ImGuiID id = ImGui::GetItemID();

	if (ImGui::IsItemActivated()) {
		s_capturedFloat3s[id] = preValue;
	}
	if (ImGui::IsItemDeactivatedAfterEdit() && s_commandManager) {
		auto it = s_capturedFloat3s.find(id);
		if (it != s_capturedFloat3s.end()) {
			Float3 newValue = *vf;
			s_commandManager->Execute(
				std::make_unique<PropertyChangeCommand<Float3>>(vf, it->second, newValue, MakePropertyDesc(label))
			);
			s_capturedFloat3s.erase(it);
		}
	}

	return changed;
}

bool ImGui::UndoDragFloat4(const char* label, float v[4], float speed,
	float v_min, float v_max, const char* format)
{
	struct Float4 { float x, y, z, w; };
	static std::unordered_map<ImGuiID, Float4> s_capturedFloat4s;

	Float4* vf = reinterpret_cast<Float4*>(v);
	Float4 preValue = *vf;

	bool changed = ImGui::DragFloat4(label, v, speed, v_min, v_max, format);
	ImGuiID id = ImGui::GetItemID();

	if (ImGui::IsItemActivated()) {
		s_capturedFloat4s[id] = preValue;
	}
	if (ImGui::IsItemDeactivatedAfterEdit() && s_commandManager) {
		auto it = s_capturedFloat4s.find(id);
		if (it != s_capturedFloat4s.end()) {
			Float4 newValue = *vf;
			s_commandManager->Execute(
				std::make_unique<PropertyChangeCommand<Float4>>(vf, it->second, newValue, MakePropertyDesc(label))
			);
			s_capturedFloat4s.erase(it);
		}
	}

	return changed;
}

bool ImGui::UndoDragInt(const char* label, int* v, float speed, int v_min, int v_max)
{
	static std::unordered_map<ImGuiID, int> s_capturedInts;

	int preValue = *v;
	bool changed = ImGui::DragInt(label, v, speed, v_min, v_max);
	ImGuiID id = ImGui::GetItemID();

	if (ImGui::IsItemActivated()) {
		s_capturedInts[id] = preValue;
	}
	if (ImGui::IsItemDeactivatedAfterEdit() && s_commandManager) {
		auto it = s_capturedInts.find(id);
		if (it != s_capturedInts.end()) {
			int newValue = *v;
			s_commandManager->Execute(
				std::make_unique<PropertyChangeCommand<int>>(v, it->second, newValue, MakePropertyDesc(label))
			);
			s_capturedInts.erase(it);
		}
	}

	return changed;
}

bool ImGui::UndoCheckbox(const char* label, bool* v)
{
	bool oldValue = *v;
	bool changed = ImGui::Checkbox(label, v);

	if (changed && s_commandManager) {
		s_commandManager->Execute(
			std::make_unique<PropertyChangeCommand<bool>>(v, oldValue, *v, MakePropertyDesc(label))
		);
	}

	return changed;
}

bool ImGui::UndoDragVec3(const char* label, Vector3& Vec3)
{
	// DragVec3 は内部で3つの DragFloat を展開するため、
	// 各フレームでいずれかの軸が編集中かどうかを追跡する。
	// 編集開始時に値をキャプチャし、編集終了時にコマンドを積む。
	struct DragVec3State {
		Vector3 captured;
		bool    wasEditing = false;
	};
	static std::unordered_map<ImGuiID, DragVec3State> s_dragVec3State;

	// 呼び出し元のウィンドウスコープ内での一意 ID
	ImGuiID id = ImGui::GetID(label);
	auto& st = s_dragVec3State[id];

	// 呼び出し前の値を保存（アクティブ化フレームのキャプチャ用）
	Vector3 preValue = Vec3;

	bool changed = ImGui::DragVec3(label, Vec3);

	// IsItemActive は DragVec3 が描画する最後のサブアイテム（Z軸）を示す。
	// X/Y 軸のドラッグ中も isEditing を維持するため、
	// 「変更があったフレーム」または「前フレームが編集中かつ何かがアクティブ」で継続中を検出。
	bool anyActive = ImGui::IsAnyItemActive();
	bool isEditing = changed || (st.wasEditing && anyActive);

	if (!st.wasEditing && isEditing) {
		// 編集セッション開始：編集前の値をキャプチャ
		st.captured = preValue;
	}
	if (st.wasEditing && !isEditing && s_commandManager) {
		// 編集セッション終了：値が変わっていたらコマンドを積む
		// &Vec3 は呼び出し元コンポーネントのメンバへの参照アドレスで安定している
		if (!(st.captured == Vec3)) {
			s_commandManager->Execute(
				std::make_unique<PropertyChangeCommand<Vector3>>(&Vec3, st.captured, Vec3, MakePropertyDesc(label))
			);
		}
	}

	st.wasEditing = isEditing;
	return changed;
}

bool ImGui::UndoColorEdit4(const char* label, float col[4], int flags)
{
	struct Float4 { float x, y, z, w; };
	static std::unordered_map<ImGuiID, Float4> s_capturedColors;

	Float4* cf = reinterpret_cast<Float4*>(col);
	Float4 preValue = *cf;

	bool changed = ImGui::ColorEdit4(label, col, flags);
	ImGuiID id = ImGui::GetItemID();

	if (ImGui::IsItemActivated()) {
		s_capturedColors[id] = preValue;
	}
	if (ImGui::IsItemDeactivatedAfterEdit() && s_commandManager) {
		auto it = s_capturedColors.find(id);
		if (it != s_capturedColors.end()) {
			Float4 newValue = *cf;
			s_commandManager->Execute(
				std::make_unique<PropertyChangeCommand<Float4>>(cf, it->second, newValue, MakePropertyDesc(label))
			);
			s_capturedColors.erase(it);
		}
	}

	return changed;
}

bool ImGui::UndoSliderFloat(const char* label, float* v, float v_min, float v_max, const char* format)
{
	static std::unordered_map<ImGuiID, float> s_capturedSliders;

	float preValue = *v;
	bool changed = ImGui::SliderFloat(label, v, v_min, v_max, format);
	ImGuiID id = ImGui::GetItemID();

	if (ImGui::IsItemActivated()) {
		s_capturedSliders[id] = preValue;
	}
	if (ImGui::IsItemDeactivatedAfterEdit() && s_commandManager) {
		auto it = s_capturedSliders.find(id);
		if (it != s_capturedSliders.end()) {
			float newValue = *v;
			if (it->second != newValue) {
				s_commandManager->Execute(
					std::make_unique<PropertyChangeCommand<float>>(v, it->second, newValue, MakePropertyDesc(label))
				);
			}
			s_capturedSliders.erase(it);
		}
	}

	return changed;
}

bool ImGui::UndoInputText(const char* label, std::string* str, size_t bufSize, int flags)
{
	static std::unordered_map<ImGuiID, std::string> s_capturedStrings;

	std::vector<char> buf(bufSize);
	buf[0] = '\0';
	strncpy(buf.data(), str->c_str(), bufSize - 1);
	buf[bufSize - 1] = '\0';

	bool changed = ImGui::InputText(label, buf.data(), bufSize, flags);
	ImGuiID id = ImGui::GetItemID();

	if (ImGui::IsItemActivated()) {
		s_capturedStrings[id] = *str;
	}
	if (changed) {
		*str = buf.data();
	}
	if (ImGui::IsItemDeactivatedAfterEdit() && s_commandManager) {
		auto it = s_capturedStrings.find(id);
		if (it != s_capturedStrings.end()) {
			std::string newValue = *str;
			if (it->second != newValue) {
				std::string capturedOld = it->second;
				s_commandManager->Execute(
					std::make_unique<PropertyChangeCommandWithSetter<std::string>>(
						[str](const std::string& val){ *str = val; },
						capturedOld, newValue, MakePropertyDesc(label)
					)
				);
			}
			s_capturedStrings.erase(it);
		}
	}

	return changed;
}

void ImGui::RecordStringChange(std::string* target, const std::string& oldValue,
	const std::string& newValue, const char* description)
{
	if (!target || oldValue == newValue) return;
	if (s_commandManager) {
		// コマンド経由で適用する（Execute() 内で *target = newValue が行われる）
		s_commandManager->Execute(
			std::make_unique<PropertyChangeCommandWithSetter<std::string>>(
				[target](const std::string& val){ *target = val; },
				oldValue, newValue, description ? description : ""
			)
		);
	} else {
		// CommandManager 未設定の場合はフォールバックで直接代入
		*target = newValue;
	}
}

