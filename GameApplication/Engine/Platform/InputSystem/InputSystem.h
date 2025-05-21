#pragma once
#include <unordered_map>
#include <array>
#include <windows.h>
#include <xinput.h>
#include "keyState.h"

constexpr int MouseButtonCount = 5; // LEFT, RIGHT, MIDDLE, XBUTTON1, XBUTTON2
constexpr int GamepadCount = 4;

class InputSystem {
public:
	InputSystem();
	~InputSystem();

	// 初期化・終了
	void Initialize(HWND window);

	// Win32メッセージ処理
	void MessageUpdateInput(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	// 毎フレーム呼び出し
	void Update();

	// キーボード
	bool IsKeyDown(int key) const;
	bool IsKeyUp(int key) const;
	bool IsKey(int key) const;

	// マウス
	bool IsMouseDown(int mouse) const;
	bool IsMouseUp(int mouse) const;
	bool IsMouse(int mouse) const;
	int GetMouseX() const;
	int GetMouseY() const;
	int GetMouseWheel() const;

	// ゲームパッド
	bool IsGamepadConnected(int id) const;
	bool GetGamepadButton(int id, int button) const;
	POINT GetGamepadLeftStick(int id) const;
	POINT GetGamepadRightStick(int id) const;
	bool GetGamepadTrigger(int id, int trigger) const;
private:
	void OnKeyDown(int key);
	void OnKeyUp(int key);
	void OnMouseMove(int x, int y);
	void OnMouseButtonDown(int button);
	void OnMouseButtonUp(int button);
	void OnMouseWheel(int delta);
	void UpdateGamepads();
	void ClipToWindow();

	std::unordered_map<int, KeyState> m_keyStates;
	MouseState m_mouseState;

	struct GamepadState {
		bool connected = false;
		XINPUT_GAMEPAD pad = {};
	};
	std::array<GamepadState, GamepadCount> m_gamepads;

	// ウィンドウ・モード管理
	HWND m_window = nullptr;
	int m_lastX = 0;
	int m_lastY = 0;
	int m_relativeX = INT32_MAX;
	int m_relativeY = INT32_MAX;
	bool m_inFocus = true;

	enum MousePositionMode {
		MousePositionMode_ABSOLUTE, MousePositionMode_RELATIVE
	} m_mode = MousePositionMode_ABSOLUTE;
};
