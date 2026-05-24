// =======================================================================
// 
// InputSystem.h
// 
// =======================================================================
#pragma once
#include <unordered_map>
#include <array>
#include <windows.h>
#include <xinput.h>
#include "Service/IService.h"
#include "keyState.h"

constexpr int m_MouseButtonCount= 5; // LEFT, RIGHT, MIDDLE, XBUTTON1, XBUTTON2
constexpr int m_GamepadCount= 4;

// ウィンドウごとの入力状態
struct WindowInputState {
	MouseState mouseState;
	std::unordered_map<int, KeyState> keyStates;
};

// キーボード・マウス・ゲームパッドの入力を管理するサービス
class InputService: public IService{
public:
	InputService();
	~InputService();

	// ウィンドウ登録・解除
	void RegisterWindow(HWND hwnd);
	void UnregisterWindow(HWND hwnd);

	// 初期化
	void Initialize(HWND window);
	void Shutdown()override {}

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
	std::unordered_map<HWND, WindowInputState> m_WindowStates;

	// ゲームパッド
	struct GamepadState {
		bool connected = false;
		XINPUT_GAMEPAD pad = {};
	};
	std::array<GamepadState, GamepadCount> gamepads;

	// ウィンドウ・モード管理（必要に応じて複数ウィンドウ対応へ拡張可）
	HWND window= nullptr;
	int lastX= 0;
	int lastY= 0;
	int relativeX= INT32_MAX;
	int relativeY= INT32_MAX;
	bool inFocus= true;

	enum mousePositionMode{
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