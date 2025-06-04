#pragma once
#include <unordered_map>
#include <array>
#include <windows.h>
#include <xinput.h>
#include "Service/IService.h"
#include "keyState.h"

constexpr int MouseButtonCount = 5; // LEFT, RIGHT, MIDDLE, XBUTTON1, XBUTTON2
constexpr int GamepadCount = 4;

// ウィンドウごとの入力状態
struct WindowInputState {
	MouseState mouseState;
	std::unordered_map<int, KeyState> keyStates;
};

class InputSystem: public IService{
public:
	InputSystem();
	~InputSystem();

	// ウィンドウ登録・解除
	void RegisterWindow(HWND hwnd);
	void UnregisterWindow(HWND hwnd);

	// 初期化
	void Initialize(HWND window);

	// Win32メッセージ処理
	void MessageUpdateInput(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	// 毎フレーム呼び出し
	void Update();

	// キーボード
	bool IsKeyDown(HWND hwnd, int key) const;
	bool IsKeyUp(HWND hwnd, int key) const;
	bool IsKey(HWND hwnd, int key) const;

	// マウス
	bool IsMouseDown(HWND hwnd, int mouse) const;
	bool IsMouseUp(HWND hwnd, int mouse) const;
	bool IsMouse(HWND hwnd, int mouse) const;
	int GetMouseX(HWND hwnd) const;
	int GetMouseY(HWND hwnd) const;
	int GetMouseWheel(HWND hwnd) const;

	// ゲームパッド（グローバル管理）
	bool IsGamepadConnected(int id) const;
	bool GetGamepadButton(int id, int button) const;
	POINT GetGamepadLeftStick(int id) const;
	POINT GetGamepadRightStick(int id) const;
	bool GetGamepadTrigger(int id, int trigger) const;

	// マウスクリップ
	void ClipToWindow(HWND hwnd);

private:
	// ウィンドウごとの入力状態
	std::unordered_map<HWND, WindowInputState> m_windowStates;

	// ゲームパッド
	struct GamepadState {
		bool connected = false;
		XINPUT_GAMEPAD pad = {};
	};
	std::array<GamepadState, GamepadCount> m_gamepads;

	// ウィンドウ・モード管理（必要に応じて複数ウィンドウ対応へ拡張可）
	HWND m_window = nullptr;
	int m_lastX = 0;
	int m_lastY = 0;
	int m_relativeX = INT32_MAX;
	int m_relativeY = INT32_MAX;
	bool m_inFocus = true;

	enum MousePositionMode {
		MousePositionMode_ABSOLUTE, MousePositionMode_RELATIVE
	} m_mode = MousePositionMode_ABSOLUTE;

	// 内部処理
	void OnKeyDown(WindowInputState& state, int key);
	void OnKeyUp(WindowInputState& state, int key);
	void OnMouseMove(WindowInputState& state, int x, int y);
	void OnMouseButtonDown(WindowInputState& state, int button);
	void OnMouseButtonUp(WindowInputState& state, int button);
	void OnMouseWheel(WindowInputState& state, int delta);
	void UpdateGamepads();
};