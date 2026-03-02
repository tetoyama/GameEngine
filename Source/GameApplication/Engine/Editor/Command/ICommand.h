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

	// true を返すと CommandManager::Execute() がコマンド実行前に Undo スタックを全クリアする。
	// エンティティ削除など、以前のコマンドのポインタを無効化する構造的な操作に使用する。
	virtual bool ClearsUndoHistory() const { return false; }

	// true を返すと CommandManager::Undo() がコマンドの Undo 実行前に Redo スタックを全クリアする。
	// エンティティ作成など、Undo でエンティティを破棄するコマンドに使用する。
	// これにより、Redo スタックに残った当該エンティティへの生ポインタが
	// 使用される前に除去され、ダングリングポインタによるクラッシュを防ぐ。
	virtual bool ClearsRedoOnUndo() const { return false; }
};
