#pragma once

// =======================================================================
// ICommand.h
// コマンドパターンの基底インターフェース
// =======================================================================

class ICommand {
public:
	virtual ~ICommand() = default;

	// コマンドを実行する
	virtual void Execute() = 0;

	// コマンドを元に戻す
	virtual void Undo() = 0;
};
