// =======================================================================
//
// systemRegistry.h
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "Interface/ISystem.h"
#include "System/Scheduler/SystemTask.h"

// Systemの所有、Lifecycle実行、SystemTaskの構築と実行を管理する。
// 現段階のExecutorは直列だが、System呼び出しはすべてTask経由へ統一する。
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
		m_tasksDirty = true;

		for(auto& system : m_systems) {
			system->Finalize();
		}
	}

	// 全SystemからTaskを再収集し、論理順序で並べる。
	// 現在は直列Executorなので、この順序がそのまま実行順になる。
	// 並列Scheduler導入後は、Access競合があるTask間の依存方向に使う。
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

	const std::vector<SystemTask>& GetTasks() {
		EnsureTasksBuilt();
		return m_tasks;
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
	void EnsureTasksBuilt() {
		if(m_tasksDirty) {
			RebuildTasks();
		}
	}

	void ExecuteTasks(SystemTaskDomain domain, float deltaTime) {
		EnsureTasksBuilt();

		const SystemTaskContext context{deltaTime};
		for(SystemTask& task : m_tasks) {
			if(task.domain != domain || !task.execute) continue;
			task.execute(context);
		}
	}

	std::vector<std::unique_ptr<ISystem>> m_systems;
	std::vector<uint64_t> m_systemRegistrationOrders;
	std::vector<SystemTask> m_tasks;
	uint64_t m_nextSystemRegistrationOrder = 0;
	bool m_tasksDirty = true;
};
