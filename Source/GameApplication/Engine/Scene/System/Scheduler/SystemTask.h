// =======================================================================
//
// SystemTask.h
//
// =======================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class ISystem;

// 一回のSchedule実行でTaskへ渡される共通情報。
// 現段階では時間だけを保持し、WorldやCommandBufferは後続Stepで追加する。
struct SystemTaskContext {
	float deltaTime = 0.0f;
};

// Fixed / Frame / Editor / Renderは実行頻度と呼び出し元が異なるため、
// 同じTask一覧の中でもDomainを明示して分類する。
enum class SystemTaskDomain : uint8_t {
	Fixed,
	Frame,
	Editor,
	Render
};

// Schedulerが実行する最小単位。
// registrationOrderはTask登録時に一度だけ発行し、Schedule再構築後も維持する。
struct SystemTask {
	ISystem* owner = nullptr;
	std::string name;
	SystemTaskDomain domain = SystemTaskDomain::Frame;
	uint64_t registrationOrder = 0;
	std::function<void(const SystemTaskContext&)> execute;
};

// Systemが自身の実行Taskを登録するためのBuilder。
// 明示的依存やAccess情報は後続Stepで拡張する。
class SystemScheduleBuilder {
public:
	SystemScheduleBuilder(
		ISystem* owner,
		std::vector<SystemTask>& tasks,
		uint64_t& nextRegistrationOrder
	)
		: m_owner(owner)
		, m_tasks(tasks)
		, m_nextRegistrationOrder(nextRegistrationOrder) {
	}

	void AddTask(
		std::string name,
		SystemTaskDomain domain,
		std::function<void(const SystemTaskContext&)> execute
	) {
		SystemTask task;
		task.owner = m_owner;
		task.name = std::move(name);
		task.domain = domain;
		task.registrationOrder = m_nextRegistrationOrder++;
		task.execute = std::move(execute);
		m_tasks.emplace_back(std::move(task));
	}

private:
	ISystem* m_owner = nullptr;
	std::vector<SystemTask>& m_tasks;
	uint64_t& m_nextRegistrationOrder;
};
