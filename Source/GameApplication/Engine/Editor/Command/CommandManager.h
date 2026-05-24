// =======================================================================
// 
// CommandManager.h
// 
// =======================================================================
#pragma once

#include "ICommand.h"
#include <stack>
#include <memory>

class CommandManager {
public:
	CommandManager() = default;
	~CommandManager() = default;

	// コマンドを実行してアンドゥスタックに積む
	void Execute(std::unique_ptr<ICommand> cmd);

	// アンドゥ（1ステップ元に戻す）
	void Undo();

	// リドゥ（1ステップやり直す）
	void Redo();

	// スタックをクリアする（シーン切り替え時など）
	void Clear();

	// コマンドをスタックに積むだけ（Execute は呼ばない）
	// 既に外部で実行済みの操作（ImGuizmo など）に使う
	void Push(std::unique_ptr<ICommand> cmd);

	bool CanUndo() const { return !m_undoStack.empty(); } // Undo 可能な履歴があるか
	bool CanRedo() const { return !m_redoStack.empty(); } // Redo 可能な履歴があるか

	// スタック先頭コマンドの操作名を返す（UI ヒント用）
	std::string PeekUndoDescription() const {
		return m_undoStack.empty() ? "" : m_undoStack.top()->GetDescription();
	}
	std::string PeekRedoDescription() const {
		return m_redoStack.empty() ? "" : m_redoStack.top()->GetDescription();
	}

private:
	void _ClearRedoStack() {
		while (!m_redoStack.empty()) m_redoStack.pop();
	}

	std::stack<std::unique_ptr<ICommand>> m_UndoStack;
	std::stack<std::unique_ptr<ICommand>> m_RedoStack;
};
