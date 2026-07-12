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

	// buttonDown は現在の保持状態。
	// buttonPressed / buttonReleased は InputService::Update() 後から
	// 次の Update() まで有効な「このフレーム」のエッジ状態。
	bool buttonDown[5] = {false, false, false, false, false};
	bool buttonPressed[5] = {false, false, false, false, false};
	bool buttonReleased[5] = {false, false, false, false, false};

	// Win32 Message 処理は PollEvents() 中に行われる。
	// Engine はその直後に InputService::Update() を呼ぶため、Message 側で
	// buttonPressed を直接立てると Update() がゲーム Update 前に消してしまう。
	// pending を Update() で公開状態へラッチすることで、Script から
	// 押下・解放の瞬間を 1 フレームだけ確実に読めるようにする。
	bool pendingPressed[5] = {false, false, false, false, false};
	bool pendingReleased[5] = {false, false, false, false, false};
};