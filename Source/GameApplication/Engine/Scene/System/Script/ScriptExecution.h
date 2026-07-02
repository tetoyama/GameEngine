// =======================================================================
//
// ScriptExecution.h
//
// =======================================================================
#pragma once

#include <cstdint>

#include "System/Scheduler/SystemTask.h"

struct ScriptCallbackOrder {
	SystemPhase phase = SystemPhase::Default;
	int32_t priority = 0;
};

// Scriptは並列実行しないが、各Callbackごとに論理順序を設定できる。
struct ScriptExecutionSettings {
	ScriptCallbackOrder fixed;
	ScriptCallbackOrder frame;
	ScriptCallbackOrder editor;
	ScriptCallbackOrder render;
	uint64_t registrationOrder = 0;

	ScriptCallbackOrder& GetCallbackOrder(SystemTaskDomain domain) {
		switch(domain) {
		case SystemTaskDomain::Fixed:
			return fixed;
		case SystemTaskDomain::Frame:
			return frame;
		case SystemTaskDomain::Editor:
			return editor;
		case SystemTaskDomain::Render:
			return render;
		}
		return frame;
	}

	const ScriptCallbackOrder& GetCallbackOrder(SystemTaskDomain domain) const {
		switch(domain) {
		case SystemTaskDomain::Fixed:
			return fixed;
		case SystemTaskDomain::Frame:
			return frame;
		case SystemTaskDomain::Editor:
			return editor;
		case SystemTaskDomain::Render:
			return render;
		}
		return frame;
	}

	SystemTaskOrder GetOrder(SystemTaskDomain domain) const {
		const ScriptCallbackOrder& callback = GetCallbackOrder(domain);
		return {
			callback.phase,
			callback.priority,
			registrationOrder
		};
	}

	void SetOrder(
		SystemTaskDomain domain,
		SystemPhase phase,
		int32_t priority
	) {
		ScriptCallbackOrder& callback = GetCallbackOrder(domain);
		callback.phase = phase;
		callback.priority = priority;
	}
};

inline bool IsScriptOrderEarlier(
	const SystemTaskOrder& lhs,
	const SystemTaskOrder& rhs
) {
	if(lhs.phase != rhs.phase) {
		return static_cast<int32_t>(lhs.phase) <
			static_cast<int32_t>(rhs.phase);
	}

	if(lhs.priority != rhs.priority) {
		return lhs.priority < rhs.priority;
	}

	return lhs.registrationOrder < rhs.registrationOrder;
}
