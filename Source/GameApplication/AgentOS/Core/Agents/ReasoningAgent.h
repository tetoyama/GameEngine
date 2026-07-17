// =======================================================================
//
// ReasoningAgent.h
//
// 統合済みEvidenceのみから仮説（Hypothesis）を組み立てるReasoning担当
// （構想§9）。confidenceはLLMの自己申告を採用せず、LogicGraphの決定的な式
// （ComputeConfidence）でのみ算出する。
//
// =======================================================================
#pragma once

#include "AgentContext.h"
#include "../Logic/LogicGraph.h"

namespace agentos {

class ReasoningAgent {
public:
	// builtEvidenceJson: EvidenceBuilder::ToJson()相当（"evidences"配列にidを含む）。
	// graphOutへ仮説ノードを追加していく。rawOutが非nullならLLM生出力を格納する。
	static Result Run(AgentContext& ctx, const Json& builtEvidenceJson, LogicGraph* graphOut, Json* rawOut = nullptr);
};

} // namespace agentos
