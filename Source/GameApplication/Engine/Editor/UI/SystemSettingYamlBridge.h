// =======================================================================
//
// SystemSettingYamlBridge.h
//
// SystemSetting.cppからSchedule YAML出力UIを最小依存で呼び出す。
//
// =======================================================================
#pragma once

#include "Editor/UI/ScheduleProfileYamlExportUI.h"

namespace SystemSettingYamlBridge {

inline void Draw(
	SystemRegistry& registry,
	const ScheduleProfilerViewState& state
) {
	ScheduleProfileYamlExportUI::Draw(registry, state);
}

} // namespace SystemSettingYamlBridge
