// =======================================================================
//
// SynthesisAgent.h
//
// 確定Evidence/仮説/停止情報のみから人間向けMarkdown報告を作るSynthesis担当
// （構想§9）。LLM出力が得られない場合でも、決定的なフォールバック報告を
// 組み立てることでセッション全体を失敗させない。
//
// =======================================================================
#pragma once

#include <string>

#include "AgentContext.h"

namespace agentos {

class SynthesisAgent {
public:
	// builtEvidence: EvidenceBuilder::ToJson()相当。
	// rankedHypotheses: LogicGraph::ToJson()相当。
	// stopInfo: EarlyStopping/Orchestratorが決定した停止理由等。
	// 常にResult::Okを返す（LLM失敗時はフォールバック報告を使う）。
	static Result Run(AgentContext& ctx, const Json& builtEvidence, const Json& rankedHypotheses,
	                   const Json& stopInfo, std::string* reportOut);
};

} // namespace agentos
