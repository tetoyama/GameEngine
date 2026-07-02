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

#include "System/Scheduler/SystemAccess.h"

class ISystem;
struct JobThreadContext;

// 一回のSchedule実行でTaskへ渡される共通情報。
// jobContextはParallel Executorが実行Threadごとに設定する。
// JobSystem停止中の直列フォールバックではnullptrとなる。
struct SystemTaskContext {
	float deltaTime = 0.0f;
	JobThreadContext* jobContext = nullptr;
};

// Fixed / Frame / Editor / Renderは実行頻度と呼び出し元が異なるため、
// 同じTask一覧の中でもDomainを明示して分類する。
enum class SystemTaskDomain : uint8_t {
	Fixed,
	Frame,
	Editor,
	Render
};

// PhaseはDomain内の大きな論理順序を表す。
// Barrierではなく、Access競合が発生したときの依存方向に使う。
enum class SystemPhase : int32_t {
	Earliest = -2000,
	Early = -1000,
	Default = 0,
	Late = 1000,
	Latest = 2000
};

struct SystemTaskOrder {
	SystemPhase phase = SystemPhase::Default;
	int32_t priority = 0;
	uint64_t registrationOrder = 0;
};

// Schedulerが実行する最小単位。
struct SystemTask {
	ISystem* owner = nullptr;
	std::string name;
	SystemTaskDomain domain = SystemTaskDomain::Frame;
	SystemTaskOrder order;
	SystemAccess access;
	ThreadAffinity threadAffinity = ThreadAffinity::MainThread;
	std::function<void(const SystemTaskContext&)> execute;
};

// Systemが自身の実行Taskを登録するためのBuilder。
// registrationOrderはSystem登録順とSystem内Task順から決定するため、
// Scheduleを再構築しても同じ登録順を維持できる。
class SystemScheduleBuilder {
public:
	SystemScheduleBuilder(
		ISystem* owner,
		uint64_t systemRegistrationOrder,
		std::vector<SystemTask>& tasks
	)
		: m_owner(owner)
		, m_systemRegistrationOrder(systemRegistrationOrder)
		, m_tasks(tasks) {
	}

	// 詳細Access未移行の既存System向け。
	// MainThread / WorldExclusiveとして安全側に登録する。
	void AddTask(
		std::string name,
		SystemTaskDomain domain,
		std::function<void(const SystemTaskContext&)> execute
	) {
		AddTask(
			std::move(name),
			domain,
			SystemPhase::Default,
			0,
			SystemAccess::LegacyExclusive(),
			ThreadAffinity::MainThread,
			std::move(execute)
		);
	}

	void AddTask(
		std::string name,
		SystemTaskDomain domain,
		SystemPhase phase,
		int32_t priority,
		std::function<void(const SystemTaskContext&)> execute
	) {
		AddTask(
			std::move(name),
			domain,
			phase,
			priority,
			SystemAccess::LegacyExclusive(),
			ThreadAffinity::MainThread,
			std::move(execute)
		);
	}

	// Query型が宣言するRead / Write ComponentからSystemAccessを自動生成する。
	// Query外の構造変更はstructuralAccessで明示する。
	template<typename QueryType>
	void AddQueryTask(
		std::string name,
		SystemTaskDomain domain,
		SystemPhase phase,
		int32_t priority,
		StructuralAccess structuralAccess,
		ThreadAffinity threadAffinity,
		std::function<void(const SystemTaskContext&)> execute
	) {
		SystemAccess access = QueryType::BuildSystemAccess();
		access.SetStructuralAccess(structuralAccess);

		AddTask(
			std::move(name),
			domain,
			phase,
			priority,
			std::move(access),
			threadAffinity,
			std::move(execute)
		);
	}

	void AddTask(
		std::string name,
		SystemTaskDomain domain,
		SystemPhase phase,
		int32_t priority,
		SystemAccess access,
		ThreadAffinity threadAffinity,
		std::function<void(const SystemTaskContext&)> execute
	) {
		SystemTask task;
		task.owner = m_owner;
		task.name = std::move(name);
		task.domain = domain;
		task.order.phase = phase;
		task.order.priority = priority;
		task.order.registrationOrder =
			(m_systemRegistrationOrder << 32) |
			static_cast<uint64_t>(m_localTaskOrder++);
		task.access = std::move(access);
		task.threadAffinity = threadAffinity;
		task.execute = std::move(execute);
		m_tasks.emplace_back(std::move(task));
	}

private:
	ISystem* m_owner = nullptr;
	uint64_t m_systemRegistrationOrder = 0;
	uint32_t m_localTaskOrder = 0;
	std::vector<SystemTask>& m_tasks;
};