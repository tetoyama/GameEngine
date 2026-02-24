#pragma once
// -----------------------------------------------------------------------
// ImGuiFunc.h
// -----------------------------------------------------------------------

// 前方宣言
class Vector3;

namespace ImGui{

	// ImGuiで自作Vector3をドラッグ可能にする関数
	bool DragVec3(const char* label, Vector3& Vec3, bool readOnly = false);

	// 90度回転テキスト描画ヘルパー
	void DrawVerticalText(const char* text);
}
