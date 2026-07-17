// =======================================================================
//
// PromptTemplates.h
//
// 各Agent（Intake/Planner/Worker/Reasoning/Critic/Synthesis）向けの
// プロンプトを組み立てる（構想§9）。ローカル9Bモデルで運用するため、
// プロンプトは簡潔に保ち、system側で厳格なJSON出力契約を明記する。
//
// =======================================================================
#pragma once

#include <string>

#include "../Json.h"

namespace agentos {

struct PromptPair {
	std::string system;
	std::string user;
};

namespace prompts {

// dump(2)した文字列をmaxChars程度に切り詰める（末尾に省略記号を付与）。
std::string Truncate(const std::string& text, std::size_t maxChars = 6000);

// DescribeTools()が返すTool一覧JSONを1Tool=1行の簡潔な表現へ圧縮する。
std::string CompactToolCatalog(const Json& toolCatalog);

// 会話履歴 + 現在入力 → standaloneな解決済み要求。
// conversationContext:
// {summary:string, recentTurns:[{user:string,assistant:string}], totalTurns:integer}
PromptPair Intake(
	const std::string& userRequest,
	const Json& conversationContext = Json::object());

// conversation fast path。履歴を参照し、ユーザー/アシスタントとScene Entityの
// 名前空間を混同しない。
PromptPair DirectReply(
	const std::string& userRequest,
	const std::string& compactToolCatalog,
	const Json& conversationContext = Json::object());

// ユーザー要求 + 実行したTool名 + Tool実行結果 → {reply:string}
PromptPair FormatToolResult(
	const std::string& userRequest,
	const std::string& toolName,
	const Json& payload);

// 古いConversation Turnを累積要約へ畳み込む。
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
