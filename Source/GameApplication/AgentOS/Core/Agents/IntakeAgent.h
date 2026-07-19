// =======================================================================
//
// IntakeAgent.h
//
// 会話履歴と現在入力からstandaloneなresolvedRequestを生成し、
// goal/symptoms/constraints/requiredCapabilitiesへ分解するIntake担当。
//
// =======================================================================
#pragma once

#include <string>

#include "AgentContext.h"

namespace agentos {

class IntakeAgent {
public:
	static Result Run(
		AgentContext& ctx,
		const std::string& userRequest,
		const Json& conversationContext,
		Json* intakeOut);

	// 履歴なしの既存呼び出し・テスト向け互換オーバーロード。
	static Result Run(AgentContext& ctx, const std::string& userRequest, Json* intakeOut) {
		return Run(ctx, userRequest, Json::object(), intakeOut);
	}
};

} // namespace agentos
