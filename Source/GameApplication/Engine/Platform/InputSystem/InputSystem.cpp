#include "InputSystem.h"
#include <windowsx.h>
#include <cassert>
#include <winerror.h>
#include <memory>
#include <algorithm>
#include <cmath>
#include <Xinput.h>

#pragma comment(lib, "xinput.lib")
InputSystem::InputSystem(){
	m_mouseState = MouseState{};
	for(auto& g : m_gamepads) g = GamepadState{};
}

InputSystem::~InputSystem(){
}

void InputSystem::Initialize(HWND window){
	m_window = window;
	m_mode = MousePositionMode_ABSOLUTE;
	m_lastX = m_lastY = 0;
	m_relativeX = m_relativeY = INT32_MAX;
	m_inFocus = true;
	m_mouseState = MouseState{};
	m_keyStates.clear();
}

void InputSystem::MessageUpdateInput(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){
	bool down = false;

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
			OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_LBUTTONDOWN:
			OnMouseButtonDown(0);
			break;
		case WM_LBUTTONUP:
			OnMouseButtonUp(0);
			break;
		case WM_RBUTTONDOWN:
			OnMouseButtonDown(1);
			break;
		case WM_RBUTTONUP:
			OnMouseButtonUp(1);
			break;
		case WM_MBUTTONDOWN:
			OnMouseButtonDown(2);
			break;
		case WM_MBUTTONUP:
			OnMouseButtonUp(2);
			break;
		case WM_XBUTTONDOWN:
			if(GET_XBUTTON_WPARAM(wParam) == XBUTTON1) OnMouseButtonDown(3);
			if(GET_XBUTTON_WPARAM(wParam) == XBUTTON2) OnMouseButtonDown(4);
			break;
		case WM_XBUTTONUP:
			if(GET_XBUTTON_WPARAM(wParam) == XBUTTON1) OnMouseButtonUp(3);
			if(GET_XBUTTON_WPARAM(wParam) == XBUTTON2) OnMouseButtonUp(4);
			break;
		case WM_MOUSEWHEEL:
			OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
			break;
		default:
			break;
	}

	// キーコード変換
	int vk = (int)wParam;
	switch(vk){
		case VK_SHIFT:
			vk = (int)MapVirtualKey(((unsigned int)lParam & 0x00ff0000) >> 16u, MAPVK_VSC_TO_VK_EX);
			if(!down){
				OnKeyUp(VK_LSHIFT);
				OnKeyUp(VK_RSHIFT);
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
		OnKeyDown(vk);
	} else if(message == WM_KEYUP || message == WM_SYSKEYUP){
		OnKeyUp(vk);
	}
}

void InputSystem::Update(){
	// マウスボタンの状態を更新
	for(int i = 0; i < MouseButtonCount; ++i){
		if(m_mouseState.buttonPressed[i]){
			m_mouseState.buttonPressed[i] = false;
		}
		if(m_mouseState.buttonReleased[i]){
			m_mouseState.buttonReleased[i] = false;
		}
	}
	// キーの状態を更新
	for(auto& state : m_keyStates){
		if(state.second.wasPressed){
			state.second.wasPressed = false;
			if(0 >= state.second.frameCount){
				state.second.frameCount = 1;
			} else{
				state.second.frameCount++;
			}
		}
		if(state.second.wasReleased){
			state.second.wasReleased = false;
			if(0 < state.second.frameCount){
				state.second.frameCount = 0;
			} else{
				state.second.frameCount--;
			}
		}
	}
	UpdateGamepads();

}

bool InputSystem::IsKeyDown(int key) const{
	auto it = m_keyStates.find(key); // キーが存在するか確認
	if(it != m_keyStates.end()){
		return (it->second.frameCount == 1);
	}
	return false; // 存在しない場合は false を返す
}

bool InputSystem::IsKeyUp(int key) const{
	auto it = m_keyStates.find(key); // キーが存在するか確認
	if(it != m_keyStates.end()){
		return (it->second.frameCount == 0);
	}
	return false; // 存在しない場合は false を返す
}

bool InputSystem::IsKey(int key) const{
	auto it = m_keyStates.find(key); // キーが存在するか確認
	if(it != m_keyStates.end()){
		return it->second.isDown;
	}
	return false; // 存在しない場合は false を返す
}

bool InputSystem::IsMouseDown(int button) const{
	if(button < 0 || button >= MouseButtonCount) return false;
	return m_mouseState.buttonDown[button];
}
bool InputSystem::IsMouseUp(int button) const{
	if(button < 0 || button >= MouseButtonCount) return false;
	return !m_mouseState.buttonDown[button];
}
bool InputSystem::IsMouse(int button) const{
	if(button < 0 || button >= MouseButtonCount) return false;
	return m_mouseState.buttonPressed[button];
}

void InputSystem::OnKeyDown(int key){
	auto& state = m_keyStates[key];
	if(!state.isDown){
		state.wasPressed = true;
	} else{
		state.wasPressed = false;
	}
	state.isDown = true;
	state.wasReleased = false;
}

void InputSystem::OnKeyUp(int key){
	auto& state = m_keyStates[key];
	state.isDown = false;
	state.wasReleased = true;
	state.wasPressed = false;
}

void InputSystem::OnMouseMove(int x, int y){
	m_mouseState.x = x;
	m_mouseState.y = y;
}

void InputSystem::OnMouseButtonDown(int button){
	if(button < 0 || button >= MouseButtonCount) return;
	if(!m_mouseState.buttonDown[button]){
		m_mouseState.buttonPressed[button] = true;
	} else{
		m_mouseState.buttonPressed[button] = false;
	}
	m_mouseState.buttonDown[button] = true;
	m_mouseState.buttonReleased[button] = false;
}

void InputSystem::OnMouseButtonUp(int button){
	if(button < 0 || button >= MouseButtonCount) return;
	m_mouseState.buttonDown[button] = false;
	m_mouseState.buttonReleased[button] = true;
	m_mouseState.buttonPressed[button] = false;
}

void InputSystem::OnMouseWheel(int delta){
	m_mouseState.wheel += delta;
}

void InputSystem::UpdateGamepads(){
	for(int i = 0; i < GamepadCount; ++i){
		XINPUT_STATE state;
		DWORD result = XInputGetState(i, &state);
		m_gamepads[i].connected = (result == ERROR_SUCCESS);
		if(m_gamepads[i].connected){
			m_gamepads[i].pad = state.Gamepad;
		} else{
			ZeroMemory(&m_gamepads[i].pad, sizeof(XINPUT_GAMEPAD));
		}
	}
}

void InputSystem::ClipToWindow(){
	RECT rect;
	GetClientRect(m_window, &rect);
	ClipCursor(&rect);
}

int InputSystem::GetMouseX() const{
	return m_mouseState.x;
}
int InputSystem::GetMouseY() const{
	return m_mouseState.y;
}
int InputSystem::GetMouseWheel() const{
	return m_mouseState.wheel;
}
bool InputSystem::IsGamepadConnected(int id) const{
	if(id < 0 || id >= GamepadCount) return false;
	return m_gamepads[id].connected;
}
bool InputSystem::GetGamepadButton(int id, int button) const{
	if(id < 0 || id >= GamepadCount) return false;
	return (m_gamepads[id].pad.wButtons & button) != 0;
}
POINT InputSystem::GetGamepadLeftStick(int id) const{
	POINT pt = {0, 0};
	if(id < 0 || id >= GamepadCount || !m_gamepads[id].connected) return pt;
	pt.x = m_gamepads[id].pad.sThumbLX;
	pt.y = m_gamepads[id].pad.sThumbLY;
	if(std::abs(pt.x) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) pt.x = 0;
	if(std::abs(pt.y) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) pt.y = 0;
	return pt;
}
POINT InputSystem::GetGamepadRightStick(int id) const{
	POINT pt = {0, 0};
	if(id < 0 || id >= GamepadCount || !m_gamepads[id].connected) return pt;
	pt.x = m_gamepads[id].pad.sThumbRX;
	pt.y = m_gamepads[id].pad.sThumbRY;
	if(std::abs(pt.x) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) pt.x = 0;
	if(std::abs(pt.y) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) pt.y = 0;
	return pt;
}
bool InputSystem::GetGamepadTrigger(int id, int trigger) const{
	if(id < 0 || id >= GamepadCount) return false;
	return (m_gamepads[id].pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) || (m_gamepads[id].pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
}