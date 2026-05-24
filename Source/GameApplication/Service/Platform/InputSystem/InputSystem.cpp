// =======================================================================
// 
// InputSystem.cpp
// 
// =======================================================================
#include "InputSystem.h"
#include <windowsx.h>
#include <cassert>
#include <winerror.h>
#include <memory>
#include <algorithm>
#include <cmath>
#include <Xinput.h>

#pragma comment(lib, "xinput.lib")

InputService::InputService(){
	for(auto& g : m_gamepads) g = GamepadState{};
}

InputService::~InputService(){}

void InputService::RegisterWindow(HWND hwnd){
	m_windowStates[hwnd] = WindowInputState{};
}

void InputService::UnregisterWindow(HWND hwnd){
	m_windowStates.erase(hwnd);
}

void InputService::Initialize(HWND window){
	m_window = window;
	m_mode = MousePositionMode_ABSOLUTE;
	m_lastX = m_lastY = 0;
	m_relativeX = m_relativeY = INT32_MAX;
	m_inFocus = true;
	// 単一ウィンドウ用の初期化。マルチウィンドウでは RegisterWindow を使う。
	m_windowStates[window] = WindowInputState{};
}

void InputService::MessageUpdateInput(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){
	auto m_It= m_windowStates.find(hWnd);
	if(it == m_windowStates.end()) return;
	WindowInputState& state = it->second;

	bool m_Down= false;

	switch(message){
		case WM_SIZE:
		case WM_MOVE:
			// ウィンドウサイズ・位置の更新
			break;
		case WM_ACTIVATEAPP:
			break;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			down = true;
			break;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			break;
		case WM_INPUT:
			// RawInputによる相対マウス処理（省略可）
			break;
		case WM_MOUSEMOVE:
			OnMouseMove(state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_LBUTTONDOWN:
			OnMouseButtonDown(state, 0);
			break;
		case WM_LBUTTONUP:
			OnMouseButtonUp(state, 0);
			break;
		case WM_RBUTTONDOWN:
			OnMouseButtonDown(state, 1);
			break;
		case WM_RBUTTONUP:
			OnMouseButtonUp(state, 1);
			break;
		case WM_MBUTTONDOWN:
			OnMouseButtonDown(state, 2);
			break;
		case WM_MBUTTONUP:
			OnMouseButtonUp(state, 2);
			break;
		case WM_XBUTTONDOWN:
			if(GET_XBUTTON_WPARAM(wParam) == XBUTTON1) OnMouseButtonDown(state, 3);
			if(GET_XBUTTON_WPARAM(wParam) == XBUTTON2) OnMouseButtonDown(state, 4);
			break;
		case WM_XBUTTONUP:
			if(GET_XBUTTON_WPARAM(wParam) == XBUTTON1) OnMouseButtonUp(state, 3);
			if(GET_XBUTTON_WPARAM(wParam) == XBUTTON2) OnMouseButtonUp(state, 4);
			break;
		case WM_MOUSEWHEEL:
			OnMouseWheel(state, GET_WHEEL_DELTA_WPARAM(wParam));
			break;
		default:
			break;
	}

	// キーコード変換
	int m_Vk= (int)wParam;
	switch(vk){
		case VK_SHIFT:
			vk = (int)MapVirtualKey(((unsigned int)lParam & 0x00ff0000) >> 16u, MAPVK_VSC_TO_VK_EX);
			if(!down){
				OnKeyUp(state, VK_LSHIFT);
				OnKeyUp(state, VK_RSHIFT);
			}
			break;
		case VK_CONTROL:
			vk = ((UINT)lParam & 0x01000000) ? VK_RCONTROL : VK_LCONTROL;
			break;
		case VK_MENU:
			vk = ((UINT)lParam & 0x01000000) ? VK_RMENU : VK_LMENU;
			break;
	}

	if(message == WM_KEYDOWN || message == WM_SYSKEYDOWN){
		OnKeyDown(state, vk);
	} else if(message == WM_KEYUP || message == WM_SYSKEYUP){
		OnKeyUp(state, vk);
	}
}

void InputService::Update(){
	for(auto& winPair : m_windowStates){
		WindowInputState& state = winPair.second;
		// マウスボタンの状態を更新
		for(int i = 0; i < MouseButtonCount; ++i){
			if(state.mouseState.buttonPressed[i]){
				state.mouseState.buttonPressed[i] = false;
			}
			if(state.mouseState.buttonReleased[i]){
				state.mouseState.buttonReleased[i] = false;
			}
		}
		// キーの状態を更新
		for(auto& kstate : state.keyStates){
			if(kstate.second.wasPressed){
				//kstate.second.wasPressed = false;
				if(0 >= kstate.second.frameCount){
					kstate.second.frameCount = 1;
				} else{
					kstate.second.frameCount++;
				}
			}
			if(kstate.second.wasReleased){
				//kstate.second.wasReleased = false;
				if(0 < kstate.second.frameCount){
					kstate.second.frameCount = 0;
				} else{
					kstate.second.frameCount--;
				}
			}
		}
	}
	UpdateGamepads();
}

bool InputService::IsKeyDown(HWND hwnd, int key) const{
	auto m_It= m_windowStates.find(hwnd);
	if(it == m_windowStates.end()) return false;
	auto m_Kit= it->second.keyStates.find(key);
	if(kit == it->second.keyStates.end()) return false;
	return kit->second.frameCount == 1;
}

bool InputService::IsKeyUp(HWND hwnd, int key) const{
	auto m_It= m_windowStates.find(hwnd);
	if(it == m_windowStates.end()) return false;
	auto m_Kit= it->second.keyStates.find(key);
	if(kit == it->second.keyStates.end()) return false;
	return kit->second.frameCount == 0;
}

bool InputService::IsKey(HWND hwnd, int key) const{
	auto m_It= m_windowStates.find(hwnd);
	if(it == m_windowStates.end()) return false;
	auto m_Kit= it->second.keyStates.find(key);
	if(kit == it->second.keyStates.end()) return false;
	return kit->second.frameCount > 0;
}

bool InputService::IsMouseDown(HWND hwnd, int button) const{
	auto m_It= m_windowStates.find(hwnd);
	if(it == m_windowStates.end()) return false;
	if(button < 0 || button >= MouseButtonCount) return false;
	return it->second.mouseState.buttonDown[button];
}

bool InputService::IsMouseUp(HWND hwnd, int button) const{
	auto m_It= m_windowStates.find(hwnd);
	if(it == m_windowStates.end()) return false;
	if(button < 0 || button >= MouseButtonCount) return false;
	return !it->second.mouseState.buttonDown[button];
}

bool InputService::IsMouse(HWND hwnd, int button) const{
	auto m_It= m_windowStates.find(hwnd);
	if(it == m_windowStates.end()) return false;
	if(button < 0 || button >= MouseButtonCount) return false;
	return it->second.mouseState.buttonPressed[button];
}

int InputService::GetMouseX(HWND hwnd) const{
	auto m_It= m_windowStates.find(hwnd);
	if(it == m_windowStates.end()) return 0;
	return it->second.mouseState.x;
}

int InputService::GetMouseY(HWND hwnd) const{
	auto m_It= m_windowStates.find(hwnd);
	if(it == m_windowStates.end()) return 0;
	return it->second.mouseState.y;
}

int InputService::GetMouseWheel(HWND hwnd) const{
	auto m_It= m_windowStates.find(hwnd);
	if(it == m_windowStates.end()) return 0;
	return it->second.mouseState.wheel;
}

void InputService::OnKeyDown(WindowInputState& state, int key){
	auto& kstate = state.keyStates[key];
	if(!kstate.isDown){
		kstate.wasPressed = true;
	} else{
		kstate.wasPressed = false;
	}
	kstate.isDown = true;
	kstate.wasReleased = false;
}

void InputService::OnKeyUp(WindowInputState& state, int key){
	auto& kstate = state.keyStates[key];
	kstate.isDown = false;
	kstate.wasReleased = true;
	kstate.wasPressed = false;
}

void InputService::OnMouseMove(WindowInputState& state, int x, int y){
	state.mouseState.x = x;
	state.mouseState.y = y;
}

void InputService::OnMouseButtonDown(WindowInputState& state, int button){
	if(button < 0 || button >= MouseButtonCount) return;
	if(!state.mouseState.buttonDown[button]){
		state.mouseState.buttonPressed[button] = true;
	} else{
		state.mouseState.buttonPressed[button] = false;
	}
	state.mouseState.buttonDown[button] = true;
	state.mouseState.buttonReleased[button] = false;
}

void InputService::OnMouseButtonUp(WindowInputState& state, int button){
	if(button < 0 || button >= MouseButtonCount) return;
	state.mouseState.buttonDown[button] = false;
	state.mouseState.buttonReleased[button] = true;
	state.mouseState.buttonPressed[button] = false;
}

void InputService::OnMouseWheel(WindowInputState& state, int delta){
	state.mouseState.wheel += delta;
}

void InputService::UpdateGamepads(){
	for(int i = 0; i < GamepadCount; ++i){
		XINPUT_STATE m_State;
		DWORD m_Result= XInputGetState(i, &state);
		m_gamepads[i].connected = (pResult == ERROR_SUCCESS);
		if(m_gamepads[i].connected){
			m_gamepads[i].pad = state.Gamepad;
		} else{
			ZeroMemory(&m_gamepads[i].pad, sizeof(XINPUT_GAMEPAD));
		}
	}
}

void InputService::ClipToWindow(HWND hwnd){
	RECT m_Rect;
	GetClientRect(hwnd, &rect);
	ClipCursor(&rect);
}

bool InputService::IsGamepadConnected(int id) const{
	if(id < 0 || id >= GamepadCount) return false;
	return m_Gamepads[id].connected;
}

bool InputService::GetGamepadButton(int id, int button) const{
	if(id < 0 || id >= GamepadCount) return false;
	return (m_gamepads[id].pad.wButtons & button) != 0;
}

POINT InputService::GetGamepadLeftStick(int id) const{
	POINT m_Pt= {0, 0};
	if(id < 0 || id >= GamepadCount || !m_gamepads[id].connected) return pt;
	pt.x = m_gamepads[id].pad.sThumbLX;
	pt.y = m_gamepads[id].pad.sThumbLY;
	if(std::abs(pt.x) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) pt.x = 0;
	if(std::abs(pt.y) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) pt.y = 0;
	return m_Pt;
}

POINT InputService::GetGamepadRightStick(int id) const{
	POINT m_Pt= {0, 0};
	if(id < 0 || id >= GamepadCount || !m_gamepads[id].connected) return pt;
	pt.x = m_gamepads[id].pad.sThumbRX;
	pt.y = m_gamepads[id].pad.sThumbRY;
	if(std::abs(pt.x) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) pt.x = 0;
	if(std::abs(pt.y) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) pt.y = 0;
	return m_Pt;
}

bool InputService::GetGamepadTrigger(int id, int trigger) const{
	if(id < 0 || id >= GamepadCount) return false;
	return (m_gamepads[id].pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) || (m_gamepads[id].pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
}
