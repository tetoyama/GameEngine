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
// 巨大なEvidence/ToolCatalogをそのままプロンプトへ流し込まないための安全弁。
std::string Truncate(const std::string& text, std::size_t maxChars = 6000);

// DescribeTools()が返すTool一覧JSONを1Tool=1行の簡潔な表現へ圧縮する。
// 形式: `- Name(arg:type, arg?:type) : description [Permission]`
// （`?`は引数が省略可能であることを示す。argumentSchema欠損・空でも動作する）
// dump(2)（~1600 prompt tokens/CPU実測）に比べ大幅に軽量化するための表現。
// 順序は入力（toolCatalog配列）の順序をそのまま保つ。
std::string CompactToolCatalog(const Json& toolCatalog);

// 自然言語の要求 → {goal, symptoms[], constraints[], requiredCapabilities[],
//                    requestType: "conversation"|"investigation"}
PromptPair Intake(const std::string& userRequest);

// ユーザー要求 + 圧縮Tool一覧 →
//   {reply: string|null, toolCall: {tool: string, arguments: object}|null, escalate: boolean}
// 会話・雑談・能力質問など、調査パイプラインを要しないrequestType="conversation"の
// ときにOrchestratorが直接使う高速パス用プロンプト（3-tier fast path の第1段）。
// - reply: 調査不要ならそのまま直接回答する。
// - toolCall: 実データが1回のRead専用Tool呼び出しで足りる場合に要求する
//   （Orchestrator側で決定的にcatalog存在・Read権限を検証してから実行する）。
// - escalate: 複数Toolや多段調査が必要な場合はtrue。通常の調査パイプラインへ委譲する。
PromptPair DirectReply(const std::string& userRequest, const std::string& compactToolCatalog);

// ユーザー要求 + 実行したTool名 + Tool実行結果 → {reply: string}
// DirectReplyがtoolCallを要求し、Orchestratorが決定的検証を経て実行した後、
// その結果をユーザー要求に沿って日本語で要約させるための高速パス第2段プロンプト
// （Reporter担当）。3-tier fast pathの「Intake後最大2回のLLM呼び出し」の2回目。
PromptPair FormatToolResult(const std::string& userRequest, const std::string& toolName, const Json& payload);

// Intake結果 + Tool一覧 → {tasks:[{taskId, type, description, dependencies[], allowedTools[], searchHints[]}]}
PromptPair Plan(const Json& intake, const Json& toolCatalog, int maxTasks);

// Task spec + Tool一覧 → Worker用 {commands:[{tool, arguments}]}（最大5件）
PromptPair GenerateQueries(const Json& taskSpec, const Json& toolCatalog);

// 統合Evidence → {hypotheses:[{description, rubricBase, supports[], contradicts[], missingEvidence[]}]}
PromptPair Reason(const Json& builtEvidence);

// 仮説 + 統合Evidence → {scores:{...}, failures[], additionalTasksSuggested[]}
PromptPair Critique(const Json& hypotheses, const Json& builtEvidence);

// 確定Evidence/仮説/停止情報 → {report:"..."}（人間向けMarkdown報告。新規調査・断定は禁止）
PromptPair Synthesize(const Json& evidence, const Json& rankedHypotheses, const Json& stopInfo);

} // namespace prompts
} // namespace agentos
