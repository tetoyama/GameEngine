// =======================================================================
//
// systemRegistry.h
//
// =======================================================================
#pragma once

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

		m_systems.emplace_back(std::move(system));
		m_tasksDirty = true;
	}

	void InitializeAll() {
		for(auto& system : m_systems) {
			system->Initialize();
		}

		// 全Systemの初期化後にTaskを構築する。
		// 将来、初期化結果に応じてTask登録を変えるSystemにも対応できる。
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
		// TaskはSystemへのポインタとlambdaを保持するため、System破棄前に解放する。
		m_tasks.clear();
		m_tasksDirty = true;

		for(auto& system : m_systems) {
			system->Finalize();
		}
	}

	// 全SystemからTaskを再収集する。
	// 登録順が同じならregistrationOrderも同じになるよう、再構築時に0から振り直す。
	void RebuildTasks() {
		m_tasks.clear();
		m_nextTaskRegistrationOrder = 0;

		for(auto& system : m_systems) {
			SystemScheduleBuilder builder(
				system.get(),
				m_tasks,
				m_nextTaskRegistrationOrder
			);
			system->RegisterTasks(builder);
		}

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
	std::vector<SystemTask> m_tasks;
	uint64_t m_nextTaskRegistrationOrder = 0;
	bool m_tasksDirty = true;
};
