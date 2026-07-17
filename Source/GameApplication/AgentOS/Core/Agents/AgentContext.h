// =======================================================================
//
// AgentContext.h
//
// 各Agent（Intake/Planner/Worker/Reasoning/Critic/Synthesis）が共有する
// 実行コンテキスト。LLM・CommandPipeline・TaskStore・Budget・Capabilityへの
// アクセスを一箇所に束ねる（構想§9）。
//
// =======================================================================
#pragma once

#include "../AgentOsTypes.h"
#include "../Json.h"
#include "../Budget/Budget.h"
#include "../Command/CommandPipeline.h"
#include "../Command/CommandTypes.h"
#include "../Llm/ILlmBackend.h"
#include "../Llm/PromptTemplates.h"
#include "../Store/TaskStore.h"

namespace agentos {

// ---------------------------------
// AgentContext
// Orchestratorが1セッション分を構築し、各Agentへ参照渡しする。
// 所有権は持たない（すべて非所有ポインタ）。
// ---------------------------------
struct AgentContext {
	ILlmBackend* llm = nullptr;
	CommandPipeline* pipeline = nullptr;
	TaskStore* store = nullptr;
	BudgetTracker* budget = nullptr;
	CapabilityToken token;
	SessionId sessionId = 0;
};

// ---------------------------------
// CallLlmJson
// PromptPairでLLMを呼び出し、Budgetを消費した上でJsonExtractorでJSONを
// 抽出するヘルパ。抽出に失敗した場合、フェンス厳守のリマインダーを追記して
// 同じシステムプロンプトで1回だけリトライする。2回目も失敗すればFail。
// Budget超過はリトライしても解消しないため即座にFailを返す。
// ---------------------------------
Result CallLlmJson(AgentContext& ctx, const PromptPair& prompt, Json* out);

} // namespace agentos
