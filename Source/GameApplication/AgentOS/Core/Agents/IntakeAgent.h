// =======================================================================
//
// IntakeAgent.h
//
// 自然言語のユーザー要求を goal/symptoms/constraints/requiredCapabilities へ
// 分解するIntake担当（構想§9）。原因の断定は行わない。
//
// =======================================================================
#pragma once

#include <string>

#include "AgentContext.h"

namespace agentos {

class IntakeAgent {
public:
	// userRequestを解析し、指定スキーマのJSONをintakeOutへ格納する。
	// "goal"（文字列）が無ければFail。配列フィールドの欠損は空配列で補う。
	static Result Run(AgentContext& ctx, const std::string& userRequest, Json* intakeOut);
};

} // namespace agentos
