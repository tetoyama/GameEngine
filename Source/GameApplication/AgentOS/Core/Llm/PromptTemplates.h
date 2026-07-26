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
std::string CurrentTurnRelation();
int CurrentRequestRevision();
Json CurrentHistoryIdentifiers();
bool CurrentRequestIsPersonalIdentityQuestion();
bool CurrentRequestIsSimpleConversation();

// Criticが提案した修正をRequest Revisionとして適用する。
// goal/resolvedRequest/constraintsのみを許可し、revisionを単調増加させる。
Result ApplyCurrentRequestPatch(const Json& requestPatch, Json* revisedIntakeOut = nullptr);

PromptPair Intake(
	const std::string& userRequest,
	const Json& conversationContext = Json::object());

PromptPair DirectReply(
	const std::string& userRequest,
	const std::string& compactToolCatalog,
	const Json& conversationContext = Json::object());

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
