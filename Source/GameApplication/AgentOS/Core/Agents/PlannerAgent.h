// =======================================================================
//
// PlannerAgent.h
//
// Intake結果とTool一覧からTask DAGを組み立てるPlanner担当（構想§9）。
// LLM出力は決定的なバリデーション（型・依存関係・循環・Tool許可）を
// 必ず通過させ、不正なPlanは拒否する。
//
// =======================================================================
#pragma once

#include "AgentContext.h"

namespace agentos {

class PlannerAgent {
public:
	// intake結果とtoolCatalog（CommandPipeline::DescribeTools()相当）からPlanを作る。
	// 最大タスク数は6件。バリデーション失敗時はエラーを添えて1回だけリトライし、
	// それでも失敗すればFailを返す。
	static Result Run(AgentContext& ctx, const Json& intake, const Json& toolCatalog, Json* planOut);
};

} // namespace agentos
