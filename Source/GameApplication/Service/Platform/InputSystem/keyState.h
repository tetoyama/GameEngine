// KeyState.h
#pragma once

struct KeyState {
	bool isDown = false;
	bool wasPressed = false;
	bool wasReleased = false;
	int frameCount = 0;
};

struct MouseState {
	int x = 0;
	int y = 0;
	int wheel = 0;
	bool buttonDown[5] = {false, false, false, false, false};
	bool buttonPressed[5] = {false, false, false, false, false};
	bool buttonReleased[5] = {false, false, false, false, false};
};
