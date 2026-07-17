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
// AgentOSServiceは同時に1 Sessionのみ実行するが、thread_localとしておくことで
// テスト並列実行時にもContextが混線しない。
void SetCurrentConversationRequestContext(
	const Json& conversationContext,
	const Json& normalizedIntake);
void ClearCurrentConversationRequestContext();
Json CurrentConversationRequestContext();
std::string CurrentResolvedRequest(const std::string& fallback = {});
bool CurrentRequestIsPersonalIdentityQuestion();

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
