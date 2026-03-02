#pragma once
// -----------------------------------------------------------------------
// ImGuiFunc.h
// -----------------------------------------------------------------------

#include <string>

// 前方宣言
class Vector3;
class CommandManager;

namespace ImGui{

	// ImGuiで自作Vector3をドラッグ可能にする関数
	bool DragVec3(const char* label, Vector3& Vec3, bool readOnly = false);

	// 90度回転テキスト描画ヘルパー
	void DrawVerticalText(const char* text);

	// -----------------------------------------------------------------------
	// アンドゥ対応ラッパー
	// CommandManager を設定すると、編集完了時に自動でコマンドを積む
	// -----------------------------------------------------------------------

	// アクティブな CommandManager を設定する（フレーム開始前に呼ぶ）
	void SetCommandManager(CommandManager* mgr);
	CommandManager* GetCommandManager();

	// アンドゥ対応 DragFloat
	bool UndoDragFloat(const char* label, float* v, float speed = 1.0f,
		float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f");

	// アンドゥ対応 DragFloat2
	bool UndoDragFloat2(const char* label, float v[2], float speed = 1.0f,
		float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f");

	// アンドゥ対応 DragFloat3
	bool UndoDragFloat3(const char* label, float v[3], float speed = 1.0f,
		float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f");

	// アンドゥ対応 DragFloat4
	bool UndoDragFloat4(const char* label, float v[4], float speed = 1.0f,
		float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f");

	// アンドゥ対応 DragInt
	bool UndoDragInt(const char* label, int* v, float speed = 1.0f,
		int v_min = 0, int v_max = 0);

	// アンドゥ対応 Checkbox
	bool UndoCheckbox(const char* label, bool* v);

	// アンドゥ対応 Vec3 ドラッグ
	bool UndoDragVec3(const char* label, Vector3& Vec3);

	// アンドゥ対応 ColorEdit4
	bool UndoColorEdit4(const char* label, float col[4], int flags = 0);

	// アンドゥ対応 SliderFloat
	bool UndoSliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f");

	// アンドゥ対応 InputText（std::string 対象）
	// 編集完了（フォーカスアウト or Enter）時にコマンドを積む
	bool UndoInputText(const char* label, std::string* str, size_t bufSize = 256,
		int flags = 0);
}
