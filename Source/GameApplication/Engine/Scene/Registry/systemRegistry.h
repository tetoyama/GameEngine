// =======================================================================
//
// systemRegistry.h
//
// =======================================================================
#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "Interface/ISystem.h"
#include "System/Job/JobSystem.h"
#include "System/Scheduler/SystemScheduleCompiler.h"
#include "System/Scheduler/SystemTask.h"

// Systemの所有、Lifecycle実行、SystemTaskの構築と実行を管理する。
// Step 13ではJobSystemの寿命を管理し、Schedule実行自体はまだ直列とする。
class SystemRegistry {
public:
	SystemRegistry() = default;
	~SystemRegistry() = default;

	void RegisterSystem(std::unique_ptr<ISystem> system) {
		if(!system) return;

		m_systemRegistrationOrders.push_back(m_nextSystemRegistrationOrder++);
		m_systems.emplace_back(std::move(system));
		m_tasksDirty = true;
	}

	void InitializeAll() {
		// System初期化処理からJobを発行できるよう、先にWorkerを起動する。
		m_jobSystem.Start();

		for(auto& system : m_systems) {
			system->Initialize();
		}

		RebuildTasks();
	}

	void StartAll() {
		for(auto& system : m_systems) {
			system->Start();
		}
	}

	void FixedUpdateAll(float fixedDeltaTime) {
		ExecuteTasks(SystemTaskDomain::Fixed, fixedDeltaTime);
	}

	void UpdateAll(float deltaTime) {
		ExecuteTasks(SystemTaskDomain::Frame, deltaTime);
	}

	void EditorUpdateAll(float deltaTime) {
		ExecuteTasks(SystemTaskDomain::Editor, deltaTime);
	}

	void DrawAll() {
		ExecuteTasks(SystemTaskDomain::Render, 0.0f);
	}

	void StopAll() {
		for(auto& system : m_systems) {
			system->Stop();
		}
	}

	void FinalizeAll() {
		m_tasks.clear();
		for(CompiledSystemSchedule& schedule : m_schedules) {
			schedule = {};
		}
		m_tasksDirty = true;

		// SystemのFinalize中に必要なJobを発行できるよう、Worker停止は最後に行う。
		for(auto& system : m_systems) {
			system->Finalize();
		}

		m_jobSystem.Stop();
	}

	// 全SystemからTaskを再収集し、Domainごとの依存Graphを構築する。
	void RebuildTasks() {
		m_tasks.clear();

		for(size_t index = 0; index < m_systems.size(); ++index) {
			SystemScheduleBuilder builder(
				m_systems[index].get(),
				m_systemRegistrationOrders[index],
				m_tasks
			);
			m_systems[index]->RegisterTasks(builder);
		}

		// Inspectorやデバッグ表示でも論理順に見えるようTask一覧自体も整列する。
		std::sort(
			m_tasks.begin(),
			m_tasks.end(),
			[](const SystemTask& lhs, const SystemTask& rhs) {
				if(lhs.domain != rhs.domain) {
					return static_cast<uint8_t>(lhs.domain) <
						static_cast<uint8_t>(rhs.domain);
				}

				if(lhs.order.phase != rhs.order.phase) {
					return static_cast<int32_t>(lhs.order.phase) <
						static_cast<int32_t>(rhs.order.phase);
				}

				if(lhs.order.priority != rhs.order.priority) {
					return lhs.order.priority < rhs.order.priority;
				}

				return lhs.order.registrationOrder < rhs.order.registrationOrder;
			}
		);

		CompileSchedules();
		m_tasksDirty = false;
	}

	template<typename T>
	T* GetSystem() {
		for(auto& system : m_systems) {
			if(auto* casted = dynamic_cast<T*>(system.get())) {
				return casted;
			}
		}
		return nullptr;
	}

	std::vector<std::unique_ptr<ISystem>>& GetSystems() {
		return m_systems;
	}

	JobSystem& GetJobSystem() noexcept {
		return m_jobSystem;
	}

	const JobSystem& GetJobSystem() const noexcept {
		return m_jobSystem;
	}

	const std::vector<SystemTask>& GetTasks() {
		EnsureTasksBuilt();
		return m_tasks;
	}

	const CompiledSystemSchedule& GetSchedule(SystemTaskDomain domain) {
		EnsureTasksBuilt();
		return m_schedules[DomainIndex(domain)];
	}

	// -------------------------
	// YAML Encode / Decode
	// -------------------------

	void EncodeAll(YAML::Node& rootNode) const {
		YAML::Node systemsNode = rootNode["Systems"];

		for(const auto& system : m_systems) {
			const char* name = system->GetSystemName();
			if(!name) continue;

			YAML::Node systemNode = system->encode();
			systemsNode[name] = systemNode;
		}

		rootNode["Systems"] = systemsNode;
	}

	void DecodeAll(const YAML::Node& rootNode) {
		if(!rootNode["Systems"]) return;

		const YAML::Node systemsNode = rootNode["Systems"];

		for(auto& system : m_systems) {
			const char* name = system->GetSystemName();
			if(!name) continue;

			const YAML::Node systemNode = systemsNode[name];
			if(!systemNode) continue;

			system->decode(systemNode);
		}

		// decode結果によってTask構成が変わるSystemへ対応する。
		m_tasksDirty = true;
	}

private:
	static constexpr size_t kDomainCount = 4;

	static constexpr size_t DomainIndex(SystemTaskDomain domain) {
		return static_cast<size_t>(domain);
	}

	void EnsureTasksBuilt() {
		if(m_tasksDirty) {
			RebuildTasks();
		}
	}

	void CompileSchedules() {
		m_schedules[DomainIndex(SystemTaskDomain::Fixed)] =
			SystemScheduleCompiler::Compile(m_tasks, SystemTaskDomain::Fixed);
		m_schedules[DomainIndex(SystemTaskDomain::Frame)] =
			SystemScheduleCompiler::Compile(m_tasks, SystemTaskDomain::Frame);
		m_schedules[DomainIndex(SystemTaskDomain::Editor)] =
			SystemScheduleCompiler::Compile(m_tasks, SystemTaskDomain::Editor);
		m_schedules[DomainIndex(SystemTaskDomain::Render)] =
			SystemScheduleCompiler::Compile(m_tasks, SystemTaskDomain::Render);
	}

	void ExecuteTasks(SystemTaskDomain domain, float deltaTime) {
		EnsureTasksBuilt();

		const CompiledSystemSchedule& schedule =
			m_schedules[DomainIndex(domain)];
		assert(schedule.valid && "Compiled system schedule is invalid");
		if(!schedule.valid) return;

		const SystemTaskContext context{deltaTime};
		for(size_t taskIndex : schedule.executionOrder) {
			SystemTask& task = m_tasks[taskIndex];
			if(task.execute) {
				task.execute(context);
			}
		}
	}

	std::vector<std::unique_ptr<ISystem>> m_systems;
	std::vector<uint64_t> m_systemRegistrationOrders;
	std::vector<SystemTask> m_tasks;
	std::array<CompiledSystemSchedule, kDomainCount> m_schedules;
	JobSystem m_jobSystem;
	uint64_t m_nextSystemRegistrationOrder = 0;
	bool m_tasksDirty = true;
};