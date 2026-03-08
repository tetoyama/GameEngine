// =======================================================================
// 
// keyState.h
// 
// =======================================================================
#pragma once

// キーボード 1 キー分の入力状態を保持する
struct KeyState {
	bool isDown = false;      // 現在押下中か
	bool wasPressed = false;  // このフレームで押されたか
	bool wasReleased = false; // このフレームで離されたか
	int frameCount = 0;       // 押下継続フレーム数
};

// マウス入力の座標・ホイール・各ボタン状態をまとめて保持する
struct MouseState {
	int x = 0; // クライアント座標 X
	int y = 0; // クライアント座標 Y
	int wheel = 0; // ホイール累積量
	bool buttonDown[5] = {false, false, false, false, false};           // 現在押下中のボタン状態
	bool buttonPressed[5] = {false, false, false, false, false};        // このフレームで押されたボタン状態
	bool buttonReleased[5] = {false, false, false, false, false};       // このフレームで離されたボタン状態
};
