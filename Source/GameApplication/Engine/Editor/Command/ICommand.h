#pragma once

// =======================================================================
// ICommand.h
// コマンドパターンの基底インターフェース
// =======================================================================

#include <string>

class ICommand {
public:
	virtual ~ICommand() = default;

	// コマンドを実行する
	virtual void Execute() = 0;

	// コマンドを元に戻す
	virtual void Undo() = 0;

	// UI 表示用の操作名（オーバーライド任意）
	virtual std::string GetDescription() const { return ""; }
};
