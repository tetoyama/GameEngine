// =======================================================================
//
// PromptTemplates.h
//
// =======================================================================
#pragma once

#include <string>

#include "../AgentOsTypes.h"
#include "../Json.h"

namespace agentos {

struct PromptPair {
	std::string system;
	std::string user;
};

namespace prompts {

std::string Truncate(const std::string& text, std::size_t maxChars = 6000);
std::string CompactToolCatalog(const Json& toolCatalog);

// Intakeで解決した会話Contextを、同一Worker threadの後続Agentへ共有する。
// Conversation Storeの全原文ではなく、Context Retrieverが今回のTurnに必要と
// 判定したselected contextだけを設定する。
void SetCurrentConversationRequestContext(
	const Json& conversationContext,
	const Json& normalizedIntake);

// Plannerが受け取ったTool Catalogを同一RunSessionのCritic/Repairへ引き継ぐ。
// thread_localで保持し、ClearCurrentConversationRequestContext()で必ず破棄する。
void SetCurrentToolCatalog(const Json& toolCatalog);
Json CurrentToolCatalog();

void ClearCurrentConversationRequestContext();
Json CurrentConversationRequestContext();
std::string CurrentResolvedRequest(const std::string& fallback = {});

// ---------------------------------
// 要求の「権威ある表現」
// ---------------------------------
// resolvedRequestはIntakeが生成した自由文であり、参照解決だけでなく
// 言い換え・装飾も混ざる。実機では「SqliteDb::Prepareの実装を見せて」が
// 「SqliteDb::Prepare メソッドの実装コードを表示する」へ書き換えられ、
// Intakeが足しただけの「メソッド」「コード」を決定的ゲートが
// ユーザ指定の調査対象と解釈して、達成済みの調査をhard failにしていた。
//
// 目的が達成されたかを判定する側は、必ず以下を見ること。
//   CurrentUserInput()        : ユーザが実際に書いた文。最終的な正解の基準。
//   CurrentTargetConcept()    : Intakeが構造化して確定させた対象。
//   CurrentResolvedEntityName(): 同上（Entity名）。
// 参照解決でIntakeが新しい語を持ち込んでよい経路は構造化フィールドだけであり、
// 自由文の言い換えで持ち込んだ語に権威を与えてはいけない。
std::string CurrentUserInput(const std::string& fallback = {});
std::string CurrentTargetConcept();
std::string CurrentResolvedEntityName();

std::string CurrentTurnRelation();
int CurrentRequestRevision();
Json CurrentHistoryIdentifiers();

// Criticが提案した修正をRequest Revisionとして適用する。
// goal/resolvedRequest/constraintsのみを許可し、revisionを単調増加させる。
Result ApplyCurrentRequestPatch(const Json& requestPatch, Json* revisedIntakeOut = nullptr);

// Respondツールが参照する、現時点までの統合Evidence。
// Toolは AgentContext を受け取れないため、worker thread単位で共有する。
// 会話応答は「Evidenceに載っていることしか断定しない」ので、これが根拠の全量になる。
void SetCurrentBuiltEvidence(const Json& builtEvidence);
Json CurrentBuiltEvidence();

PromptPair Intake(
	const std::string& userRequest,
	const Json& conversationContext = Json::object());

// Respondツール本体が使うプロンプト。
// 会話応答は「観測」ではなく生成物なので、Evidenceに無いことを断定させない。
// 分からないことは分からないと言わせる（それ自体が正しい応答になり得る）。
PromptPair Respond(const std::string& instruction, const Json& dependencyEvidence);

PromptPair FormatToolResult(
	const std::string& userRequest,
	const std::string& toolName,
	const Json& payload);

PromptPair CompressConversationMemory(
	const std::string& existingSummary,
	const Json& turnsToCompress);

PromptPair Plan(const Json& intake, const Json& toolCatalog, int maxTasks);
PromptPair GenerateQueries(const Json& taskSpec, const Json& toolCatalog);
PromptPair Reason(const Json& builtEvidence);
PromptPair Critique(const Json& hypotheses, const Json& builtEvidence);
PromptPair Synthesize(const Json& evidence, const Json& rankedHypotheses, const Json& stopInfo);

} // namespace prompts
} // namespace agentos
