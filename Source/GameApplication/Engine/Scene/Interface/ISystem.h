// =======================================================================
//
// ISystem.h
//
// =======================================================================
#pragma once

#include <string>
#include <backends/yaml-cpp/yaml.h>

#include "System/Scheduler/SystemTask.h"

// ECSのSystem基底インターフェース。
// Systemは状態と設定を所有し、Schedulerへ一つ以上のSystemTaskを登録する。
class ISystem {
public:
	ISystem() = default;
	virtual ~ISystem() = default;

	virtual void Initialize() {}
	virtual void Finalize() {}

	virtual void Start() {}
	virtual void Update(float deltaTime) {}
	virtual void FixedUpdate(float fixedDeltaTime) {}
	virtual void Draw() {}
	virtual void EditorUpdate(float deltaTime) {}
	virtual void Stop() {}

	virtual void SystemSetting() {}

	// SystemSetting() に有効なUI実装があるかどうかを返す。
	virtual bool HasSystemSetting() const { return false; }

	virtual const char* GetSystemName() const {
		return nullptr;
	}

	// 既存System向けの互換実装。
	// 各仮想コールバックをDomain別Taskとして登録するため、既存Systemは変更不要。
	// 一つのSystemを複数Taskへ分割する場合は、この関数をoverrideする。
	virtual void RegisterTasks(SystemScheduleBuilder& builder) {
		const char* rawName = GetSystemName();
		const std::string systemName = rawName ? rawName : "UnnamedSystem";

		builder.AddTask(
			systemName + ".FixedUpdate",
			SystemTaskDomain::Fixed,
			[this](const SystemTaskContext& context) {
				FixedUpdate(context.deltaTime);
			}
		);

		builder.AddTask(
			systemName + ".Update",
			SystemTaskDomain::Frame,
			[this](const SystemTaskContext& context) {
				Update(context.deltaTime);
			}
		);

		builder.AddTask(
			systemName + ".EditorUpdate",
			SystemTaskDomain::Editor,
			[this](const SystemTaskContext& context) {
				EditorUpdate(context.deltaTime);
			}
		);

		builder.AddTask(
			systemName + ".Draw",
			SystemTaskDomain::Render,
			[this](const SystemTaskContext&) {
				Draw();
			}
		);
	}

	virtual YAML::Node encode() { return YAML::Node(); }
	virtual bool decode(const YAML::Node& node) { return true; }
};
