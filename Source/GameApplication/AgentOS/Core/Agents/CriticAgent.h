// =======================================================================
//
// CriticAgent.h
//
// 仮説とEvidenceを検証するCritic担当（構想§9）。
// pass/failは必ずプログラム側で決定的に算出する（LLMの所見はllmScoresへ
// advisoryとして保存するのみで、判定には使わない）。
//
// =======================================================================
#pragma once

#include <string>
#include <vector>

#include "AgentContext.h"

namespace agentos {

struct CriticVerdict {
	double programmaticScore = 0.0;
	Json llmScores = Json::object();
	std::vector<std::string> failures;
	Json additionalTasks = Json::array();
	bool pass = false;
};

class CriticAgent {
public:
	// rankedHypotheses: LogicGraph::ToJson()相当（{"hypotheses":[...]}）。
	// builtEvidence: EvidenceBuilder::ToJson()相当。
	// LLM呼び出しが失敗しても（advisoryに過ぎないため）Failにはせず、
	// プログラム採点のみで劣化した検証結果を返す。
	static Result Run(AgentContext& ctx, const Json& rankedHypotheses, const Json& builtEvidence, CriticVerdict* out);
};

} // namespace agentos
