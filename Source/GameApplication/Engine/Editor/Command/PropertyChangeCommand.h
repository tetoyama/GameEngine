// =======================================================================
// 
// PropertyChangeCommand.h
// 
// =======================================================================
#pragma once

#include "ICommand.h"
#include <functional>

// 汎用プロパティ変更コマンド
template<typename T>
class PropertyChangeCommand : public ICommand {
public:
	PropertyChangeCommand(T* target, T oldValue, T newValue, std::string description = "")
		: m_target(target)
		, m_oldValue(oldValue)
		, m_newValue(newValue)
		, m_description(std::move(description))
	{
	}

	void Execute() override {
		if (m_target) *m_target = m_newValue;
	}

	void Undo() override {
		if (m_target) *m_target = m_oldValue;
	}

	std::string GetDescription() const override { return m_description; }

private:
	T*          m_target;
	T m_OldValue;
	T m_NewValue;
	std::string m_Description;
};

// カスタムセッター付きプロパティ変更コマンド（クォータニオン等、setter経由で設定するケース用）
template<typename T>
class PropertyChangeCommandWithSetter : public ICommand {
public:
	using Setter = std::function<void(const T&)>;

	PropertyChangeCommandWithSetter(Setter setter, T oldValue, T newValue, std::string description = "")
		: m_setter(setter)
		, m_oldValue(oldValue)
		, m_newValue(newValue)
		, m_description(std::move(description))
	{
	}

	void Execute() override {
		if (m_setter) m_setter(m_newValue);
	}

	void Undo() override {
		if (m_setter) m_setter(m_oldValue);
	}

	std::string GetDescription() const override { return m_description; }

private:
	Setter m_Setter;
	T m_OldValue;
	T m_NewValue;
	std::string m_Description;
};
