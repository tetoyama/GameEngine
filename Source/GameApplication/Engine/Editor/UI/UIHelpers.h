// =======================================================================
// 
// UIHelpers.h
// 
// =======================================================================
#pragma once

#include <Backends/ImGui/imgui.h>
#include <cstring>
#include <string>

// エディタ UI で共通利用する軽量ヘルパ
namespace EditorUIHelpers {

	// ドッキング用ウィンドウ共通設定を適用する
	inline void PrepareDockWindow() {
		ImGuiWindowClass windowClass{};
		windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
		ImGui::SetNextWindowClass(&windowClass);
	}

	// メニューバー上のウィンドウ表示トグルを共通化する
	inline void ToggleWindowMenuItem(const char* label, bool& isVisible, const char* shortcut = nullptr) {
		if(ImGui::MenuItem(label, shortcut, isVisible)) {
			isVisible = !isVisible;
		}
	}

	// std::string を ImGui 用固定長バッファへ安全にコピーする
	template<size_t N>
	inline void CopyStringToBuffer(char (&buffer)[N], const std::string& value) {
		static_assert(N > 0, "buffer size must be greater than zero");
		std::strncpy(buffer, value.c_str(), N - 1);
		buffer[N - 1] = '\0';
	}

	// バッファ内容を空文字へ初期化する
	template<size_t N>
	inline void ClearBuffer(char (&buffer)[N]) {
		static_assert(N > 0, "buffer size must be greater than zero");
		buffer[0] = '\0';
	}
}
