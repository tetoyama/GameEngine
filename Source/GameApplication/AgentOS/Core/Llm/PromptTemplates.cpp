// =======================================================================
//
// PromptTemplates.cpp
//
// =======================================================================
#include "PromptTemplates.h"

namespace agentos {
namespace prompts {

std::string Truncate(const std::string& text, std::size_t maxChars) {
	if (text.size() <= maxChars) {
		return text;
	}
	return text.substr(0, maxChars) + "\n...(truncated)...";
}

namespace {

// 全Agent共通のJSON出力契約。system prompt冒頭に必ず含める。
// - 単一の```json フェンスで応答する
// - 余計なキー・フェンス外の文章は禁止
// - 不確実性は指定フィールド（confidence/missingEvidence/failures等）で表現し、
//   事実を捏造してはならない
const char* kContractJa =
	"あなたはAgentOSのサブシステムとして動作するアシスタントです。\n"
	"出力契約（厳守）:\n"
	"1. 応答は単一の```json フェンスで囲まれたJSONオブジェクトのみとすること。\n"
	"2. フェンスの外に文章を書かないこと。指定されたキー以外を追加しないこと。\n"
	"3. 不確実な点は指定フィールド（confidence/missingEvidence/failures等）で表現し、\n"
	"   存在しない事実やEvidenceを捏造してはならない。\n";

std::string BuildSystem(const std::string& roleDescription, const std::string& schemaJson) {
	std::string s;
	s += kContractJa;
	s += "\n役割: ";
	s += roleDescription;
	s += "\n出力スキーマ:\n";
	s += schemaJson;
	s += "\n";
	// Qwen系モデルのThinkingモード抑制（soft switch）。
	// 実機ログでThinkingが1200token以上を消費しJSON到達前にタイムアウトする
	// 事例が確認されたため付与する。Qwen以外のモデルはこの行を無視するだけで実害はない。
	s += "/no_think\n";
	return s;
}

} // namespace

std::string CompactToolCatalog(const Json& toolCatalog) {
	std::string out;
	if (!toolCatalog.is_array()) {
		return out;
	}

	for (const auto& tool : toolCatalog) {
		if (!tool.is_object()) {
			continue;
		}
		const std::string name = tool.value("name", std::string());
		const std::string description = tool.value("description", std::string());
		const std::string permission = tool.value("requiredPermission", std::string());

		std::string args;
		if (tool.contains("argumentSchema") && tool.at("argumentSchema").is_object()) {
			bool first = true;
			for (const auto& item : tool.at("argumentSchema").items()) {
				if (!first) {
					args += ", ";
				}
				first = false;
				const Json& spec = item.value();
				const bool required = spec.is_object() && spec.value("required", false);
				const std::string type = spec.is_object() ? spec.value("type", std::string("any")) : std::string("any");
				args += item.key();
				if (!required) {
					args += "?";
				}
				args += ":";
				args += type;
			}
		}

		out += "- ";
		out += name;
		out += "(";
		out += args;
		out += ") : ";
		out += description;
		out += " [";
		out += permission;
		out += "]\n";
	}
	return out;
}

PromptPair Intake(const std::string& userRequest) {
	PromptPair p;
	p.system = BuildSystem(
		"ユーザーの自然言語要求を、目的・症状・制約・必要な能力へ分解し、"
		"かつ要求の種別を判定するIntake担当。\n"
		"investigation = エンジン/シーンの実データ（Entity・Component・System・Trace等）の"
		"取得・観測・調査・不具合解析を要求するもの（Toolの実行が必要なものは全てinvestigation）。\n"
		"conversation = 挨拶・雑談・AgentOS自体の能力や使い方の質問。\n"
		"例: 「このシーンのEntityを一覧して」→ investigation\n"
		"例: 「あなたは何ができますか？」→ conversation",
		"{\"goal\": string, \"symptoms\": [string], \"constraints\": [string], "
		"\"requiredCapabilities\": [string], "
		"\"requestType\": \"conversation\"|\"investigation\"}");

	p.user = std::string("以下のユーザー要求を分析し、指定スキーマのJSONで出力してください。\n\n");
	p.user += "ユーザー要求:\n";
	p.user += userRequest;
	return p;
}

PromptPair DirectReply(const std::string& userRequest, const std::string& compactToolCatalog) {
	PromptPair p;
	p.system = BuildSystem(
		"AgentOSの対話窓口として、ユーザーへ日本語で応答するDirectReply担当。"
		"調査を伴わない質問には reply で直接答える。実データが必要なら toolCall で下記Tool一覧から"
		"Read権限のToolを1つだけ要求する（実行結果は次のターンで渡される）。複数Toolや多段調査が"
		"必要なら escalate=true とする。できないことを約束しない・実行していない操作を"
		"実行したと言わないこと。",
		"{\"reply\": string|null, \"toolCall\": {\"tool\": string, \"arguments\": object}|null, "
		"\"escalate\": boolean}");

	p.user = "ユーザー要求:\n";
	p.user += userRequest;
	p.user += "\n\n利用可能なTool一覧:\n";
	p.user += compactToolCatalog;
	return p;
}

PromptPair FormatToolResult(const std::string& userRequest, const std::string& toolName, const Json& payload) {
	PromptPair p;
	p.system = BuildSystem(
		"Tool実行結果をユーザー要求に沿って日本語で要約するReporter担当。"
		"結果に無い事実を足さないこと。",
		"{\"reply\": string}");

	p.user = "ユーザー要求:\n";
	p.user += userRequest;
	p.user += "\n\n実行したTool: ";
	p.user += toolName;
	p.user += "\n\nTool実行結果:\n";
	p.user += Truncate(payload.dump(2));
	return p;
}

PromptPair Plan(const Json& intake, const Json& toolCatalog, int maxTasks) {
	PromptPair p;
	p.system = BuildSystem(
		"Intake結果とTool一覧からTask DAGを作るPlanner担当。task数は" +
			std::to_string(maxTasks) + "件以内に収めること。",
		"{\"tasks\": [{\"taskId\": string (例: \"T1\"), "
		"\"type\": \"RuntimeObservation\"|\"CodeSearch\"|\"Trace\"|\"Analysis\", "
		"\"description\": string, \"dependencies\": [string (taskIdを参照)], "
		"\"allowedTools\": [string], \"searchHints\": [string]}]}");

	p.user = "Intake結果:\n";
	p.user += Truncate(intake.dump(2));
	p.user += "\n\nTool一覧:\n";
	p.user += Truncate(CompactToolCatalog(toolCatalog));
	p.user += "\n\n最大Task数: " + std::to_string(maxTasks);
	return p;
}

PromptPair GenerateQueries(const Json& taskSpec, const Json& toolCatalog) {
	PromptPair p;
	p.system = BuildSystem(
		"割り当てられたTaskを遂行するために実行すべきTool呼び出しを提案するWorker担当。"
		"最大5件までとする。",
		"{\"commands\": [{\"tool\": string, \"arguments\": object}]}");

	p.user = "Task spec:\n";
	p.user += Truncate(taskSpec.dump(2));
	p.user += "\n\nTool一覧:\n";
	p.user += Truncate(CompactToolCatalog(toolCatalog));
	return p;
}

PromptPair Reason(const Json& builtEvidence) {
	PromptPair p;
	p.system = BuildSystem(
		"統合済みEvidenceのみから仮説を組み立てるReasoning担当。Evidenceに無い事実は書かないこと。"
		"rubricBaseは論拠の質の自己評価（0〜1）であり、最終confidenceそのものではない。",
		"{\"hypotheses\": [{\"description\": string, \"rubricBase\": number, "
		"\"supports\": [integer], \"contradicts\": [integer], \"missingEvidence\": [string]}]}");

	p.user = "統合Evidence:\n";
	p.user += Truncate(builtEvidence.dump(2));
	return p;
}

PromptPair Critique(const Json& hypotheses, const Json& builtEvidence) {
	PromptPair p;
	p.system = BuildSystem(
		"仮説とEvidenceを検証するCritic担当。各観点を0〜1で採点し、欠陥と追加調査案を挙げる。",
		"{\"scores\": {\"evidenceCoverage\": number, \"contradictionHandling\": number, "
		"\"causalCompleteness\": number, \"testability\": number}, "
		"\"failures\": [string], "
		"\"additionalTasksSuggested\": [{\"type\": string, \"description\": string}]}");

	p.user = "仮説:\n";
	p.user += Truncate(hypotheses.dump(2));
	p.user += "\n\n統合Evidence:\n";
	p.user += Truncate(builtEvidence.dump(2));
	return p;
}

PromptPair Synthesize(const Json& evidence, const Json& rankedHypotheses, const Json& stopInfo) {
	PromptPair p;
	p.system = BuildSystem(
		"確定したEvidence・仮説・停止理由のみから人間向けMarkdown報告を作るSynthesis担当。"
		"新規の調査や、Evidenceに基づかない断定は禁止。不確実な範囲は明記すること。",
		"{\"report\": string}");

	p.user = "確定Evidence:\n";
	p.user += Truncate(evidence.dump(2));
	p.user += "\n\n順位付き仮説:\n";
	p.user += Truncate(rankedHypotheses.dump(2));
	p.user += "\n\n停止情報:\n";
	p.user += Truncate(stopInfo.dump(2));
	return p;
}

} // namespace prompts
} // namespace agentos
