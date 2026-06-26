// =======================================================================
//
// ISystem.h
//
// =======================================================================
#pragma once

#include <vector>

#include <backends/yaml-cpp/yaml.h>

#include "System/Scheduler/SystemTask.h"

// ECSのSystem基底インターフェース。
// Systemは状態と設定を所有し、実行処理はRegisterTasksでSchedulerへ明示登録する。
class ISystem {
public:
	ISystem() = default;
	virtual ~ISystem() = default;

	virtual void Initialize() {}
	virtual void Finalize() {}

	// Start / Stop はScene再生状態の切り替え時に即時実行されるLifecycle処理。
	// 毎Frame/Fixed/Render/Editorの実行処理はRegisterTasksへ移行する。
	virtual void Start() {}
	virtual void Stop() {}

	virtual void SystemSetting() {}

	// SystemSetting() に有効なUI実装があるかどうかを返す。
	virtual bool HasSystemSetting() const { return false; }

	virtual const char* GetSystemName() const {
		return nullptr;
	}

	// Systemは必要なDomainだけをTaskとして明示登録する。
	// 旧式のFixedUpdate/Update/EditorUpdate/Draw互換Taskは廃止する。
	virtual void RegisterTasks(SystemScheduleBuilder& builder) {
		(void)builder;
	}

	// 大規模Systemを段階移行するための互換Hook。
	// RegisterTasksが生成したLegacy Taskを、Schedule Compile前に新Taskへ置換する。
	// 新規Systemは原則としてRegisterTasksだけで完結させる。
	virtual void MigrateRegisteredTasks(
		SystemScheduleBuilder& builder,
		std::vector<SystemTask>& tasks
	) {
		(void)builder;
		(void)tasks;
	}

	virtual YAML::Node encode() { return YAML::Node(); }
	virtual bool decode(const YAML::Node& node) { return true; }
};
