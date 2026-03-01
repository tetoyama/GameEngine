#pragma once

// =======================================================================
// PropertyChangeCommand.h
// プロパティ変更コマンド（テンプレート）
// =======================================================================

#include "ICommand.h"
#include <functional>

// 汎用プロパティ変更コマンド
template<typename T>
class PropertyChangeCommand : public ICommand {
public:
	PropertyChangeCommand(T* target, T oldValue, T newValue)
		: m_target(target)
		, m_oldValue(oldValue)
		, m_newValue(newValue)
	{
	}

	void Execute() override {
		if (m_target) *m_target = m_newValue;
	}

	void Undo() override {
		if (m_target) *m_target = m_oldValue;
	}

private:
	T*  m_target;
	T   m_oldValue;
	T   m_newValue;
};

// カスタムセッター付きプロパティ変更コマンド（クォータニオン等、setter経由で設定するケース用）
template<typename T>
class PropertyChangeCommandWithSetter : public ICommand {
public:
	using Setter = std::function<void(const T&)>;

	PropertyChangeCommandWithSetter(Setter setter, T oldValue, T newValue)
		: m_setter(setter)
		, m_oldValue(oldValue)
		, m_newValue(newValue)
	{
	}

	void Execute() override {
		if (m_setter) m_setter(m_newValue);
	}

	void Undo() override {
		if (m_setter) m_setter(m_oldValue);
	}

private:
	Setter m_setter;
	T      m_oldValue;
	T      m_newValue;
};
