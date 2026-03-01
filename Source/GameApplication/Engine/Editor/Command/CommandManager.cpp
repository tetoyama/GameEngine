// =======================================================================
// CommandManager.cpp
// =======================================================================

#include "CommandManager.h"

void CommandManager::Execute(std::unique_ptr<ICommand> cmd) {
	if (!cmd) return;

	cmd->Execute();
	m_undoStack.push(std::move(cmd));

	// 新しい操作が入ったらリドゥスタックをクリア
	while (!m_redoStack.empty()) {
		m_redoStack.pop();
	}
}

void CommandManager::Undo() {
	if (m_undoStack.empty()) return;

	auto& cmd = m_undoStack.top();
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
