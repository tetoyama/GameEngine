// =======================================================================
// CommandManager.cpp
// =======================================================================

#include "CommandManager.h"

void CommandManager::Execute(std::unique_ptr<ICommand> cmd) {
	if (!cmd) return;

	// 構造的な削除操作（エンティティ削除など）はそれ以前のコマンドのポインタを
	// 無効化するため、Undo スタックを事前にクリアする
	if (cmd->ClearsUndoHistory()) {
		while (!m_undoStack.empty()) m_undoStack.pop();
	}

	cmd->Execute();
	m_undoStack.push(std::move(cmd));

	// 新しい操作が入ったらリドゥスタックをクリア
	_ClearRedoStack();
}

void CommandManager::Push(std::unique_ptr<ICommand> cmd) {
	if (!cmd) return;
	// Execute は呼ばずにスタックに積む（操作は既に外部で適用済み）
	m_undoStack.push(std::move(cmd));
	_ClearRedoStack();
}

void CommandManager::Undo() {
	if (m_undoStack.empty()) return;

	auto& cmd = m_undoStack.top();

	// エンティティ作成系コマンドは Undo でエンティティを破棄するため、
	// Redo スタックに残った当該エンティティへの生ポインタを事前にクリアする
	if (cmd->ClearsRedoOnUndo()) {
		while (!m_redoStack.empty()) m_redoStack.pop();
	}

	cmd->Undo();
	m_redoStack.push(std::move(cmd));
	m_undoStack.pop();
}

void CommandManager::Redo() {
	if (m_redoStack.empty()) return;

	auto& cmd = m_redoStack.top();
	cmd->Execute();
	m_undoStack.push(std::move(cmd));
	m_redoStack.pop();
}

void CommandManager::Clear() {
	while (!m_undoStack.empty()) m_undoStack.pop();
	while (!m_redoStack.empty()) m_redoStack.pop();
}
