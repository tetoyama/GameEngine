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
	s += "\n/no_think\n";
	return s;
}

std::string ContextText(const Json& conversationContext) {
	if (!conversationContext.is_object() || conversationContext.empty()) {
		return "(会話履歴なし)";
	}
	return Truncate(conversationContext.dump(2), 12000);
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
				const std::string type = spec.is_object()
					? spec.value("type", std::string("any"))
					: std::string("any");
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

PromptPair Intake(const std::string& userRequest, const Json& conversationContext) {
	PromptPair p;
	p.system = BuildSystem(
		"会話履歴と現在入力から、今回の要求を単独で意味の通る形へ解決するIntake担当。\n"
		"会話解決規則（厳守）:\n"
		"- summaryは古いTurnの累積要約、recentTurnsは最近のuser/assistant最終応答ペア。\n"
		"- 『そうじゃなくて』『違う』『それ』『続けて』『前の』『今の』等は履歴を参照する。\n"
		"- 最新の明示的な訂正・否定は、過去の矛盾する条件より必ず優先する。\n"
		"- resolvedRequestは履歴を知らない別Agentでも実行できるstandaloneな要求にする。\n"
		"- ユーザー自身を指す『私/わたし/僕』、会話相手を指す『あなた』を、"
		"Scene内Entity名へ変換しない。ゲーム内対象は明示された場合だけ扱う。\n"
		"- 履歴に根拠がない参照は推測せずunresolvedReferencesへ入れる。\n"
		"requestType: investigation = Engine/Scene実データの取得・観測・解析が必要。"
		"conversation = 雑談、ユーザー/AgentOSについての質問、履歴だけで答えられる修正確認。",
		"{\"goal\": string, \"resolvedRequest\": string, "
		"\"turnRelation\": \"new\"|\"continue\"|\"correct\"|\"clarify\"|\"refer\", "
		"\"symptoms\": [string], \"constraints\": [string], "
		"\"requiredCapabilities\": [string], \"unresolvedReferences\": [string], "
		"\"requestType\": \"conversation\"|\"investigation\"}");

	p.user = "会話履歴:\n";
	p.user += ContextText(conversationContext);
	p.user += "\n\n現在のユーザー入力:\n";
	p.user += userRequest;
	p.user += "\n\n履歴を反映したresolvedRequestを生成してください。";
	return p;
}

PromptPair DirectReply(
	const std::string& userRequest,
	const std::string& compactToolCatalog,
	const Json& conversationContext) {

	PromptPair p;
	p.system = BuildSystem(
		"AgentOSの対話窓口として、会話履歴を踏まえて日本語で応答するDirectReply担当。\n"
		"- 直近の訂正や否定を優先し、前の回答を繰り返さない。\n"
		"- ユーザー自身の人物情報や『私は誰』という質問をScene Entity検索へ変換しない。\n"
		"- Toolはユーザーがゲーム内Scene/Entity/Component/System等の実データを明示的に"
		"求め、1回のRead Toolで完結する場合だけ提案する。\n"
		"- 履歴だけで回答できるならreplyを返す。複数Toolや多段調査が必要ならescalate=true。\n"
		"- 実行していない操作を実行済みと言わない。",
		"{\"reply\": string|null, \"toolCall\": {\"tool\": string, \"arguments\": object}|null, "
		"\"escalate\": boolean}");

	p.user = "会話履歴:\n";
	p.user += ContextText(conversationContext);
	p.user += "\n\n今回の解決済み要求:\n";
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

PromptPair CompressConversationMemory(
	const std::string& existingSummary,
	const Json& turnsToCompress) {

	PromptPair p;
	p.system = BuildSystem(
		"古いConversation Turnを累積会話要約へ圧縮するMemory担当。\n"
		"保存規則（厳守）:\n"
		"- 各Turnはuser入力とassistant最終応答のペアとして読む。\n"
		"- ユーザーの目的、確定仕様、好み、対象名、直近の訂正、未完了事項を保持する。\n"
		"- 後の訂正が前の内容と矛盾する場合、最新状態を正文とし、変更された事実も短く残す。\n"
		"- Toolの中間ログ、思考過程、冗長な言い回しは残さない。\n"
		"- 存在しない事実を追加しない。summaryは4000文字程度以内。",
		"{\"summary\": string}");

	p.user = "既存の累積要約:\n";
	p.user += existingSummary.empty() ? "(なし)" : Truncate(existingSummary, 6000);
	p.user += "\n\n今回要約へ畳み込むTurn:\n";
	p.user += Truncate(turnsToCompress.dump(2), 14000);
	return p;
}

PromptPair Plan(const Json& intake, const Json& toolCatalog, int maxTasks) {
	PromptPair p;
	p.system = BuildSystem(
		"Intake結果とTool一覧からTask DAGを作るPlanner担当。task数は" +
			std::to_string(maxTasks) + "件以内に収めること。\n"
		"重要な計画規則:\n"
		"- resolvedRequestと最新constraintsを正とし、訂正前の条件を復活させない。\n"
		"- 現在状態・一覧・概要の取得はスナップショット観測であり、ListEntities/ListSystems/"
		"DescribeEntity等を使う。時間変化・推移・フレーム間差分が明示されない限りWriteTraceを使わない。\n"
		"- Toolを実行するTaskはAnalysisにしない。AnalysisはallowedToolsを空配列にする。\n"
		"- 後続TaskがEntity名やComponent名を必要とする場合、先行Taskで候補を取得しdependenciesで"
		"明示する。存在を確認していない名前を計画へ埋め込まない。",
		"{\"tasks\": [{\"taskId\": string, "
		"\"type\": \"RuntimeObservation\"|\"CodeSearch\"|\"Trace\"|\"Analysis\", "
		"\"description\": string, \"dependencies\": [string], "
		"\"allowedTools\": [string], \"searchHints\": [string]}]}");

	p.user = "Intake結果:\n";
	p.user += Truncate(intake.dump(2), 10000);
	p.user += "\n\nTool一覧:\n";
	p.user += Truncate(CompactToolCatalog(toolCatalog));
	p.user += "\n\n最大Task数: " + std::to_string(maxTasks);
	return p;
}

PromptPair GenerateQueries(const Json& taskSpec, const Json& toolCatalog) {
	PromptPair p;
	p.system = BuildSystem(
		"割り当てられたTaskを遂行するために実行すべきTool呼び出しを提案するWorker担当。"
		"最大5件までとする。\n"
		"引数のGrounding規則（厳守）:\n"
		"- Task specにdependencyEvidenceがある場合、後続ToolのEntity名・Component名等は"
		"そこに実在する文字列を完全一致でコピーする。\n"
		"- 空文字、主要なEntity、対象Component、key component、TODO等の説明用プレースホルダーを"
		"引数にしない。存在しない名前を推測しない。\n"
		"- 必要な具体値をEvidenceから決められない場合は捏造せずcommandsを空配列にする。\n"
		"- allowedToolsにないToolを提案しない。",
		"{\"commands\": [{\"tool\": string, \"arguments\": object}]}");

	p.user = "Task spec:\n";
	p.user += Truncate(taskSpec.dump(2), 10000);
	p.user += "\n\nTool一覧:\n";
	p.user += Truncate(CompactToolCatalog(toolCatalog));
	return p;
}

PromptPair Reason(const Json& builtEvidence) {
	PromptPair p;
	p.system = BuildSystem(
		"統合済みEvidenceとrequestContextのみから仮説を組み立てるReasoning担当。"
		"Evidenceに無い実世界の事実は書かない。最新のresolvedRequest/constraintsを優先する。"
		"rubricBaseは論拠の質の自己評価（0〜1）であり、最終confidenceそのものではない。",
		"{\"hypotheses\": [{\"description\": string, \"rubricBase\": number, "
		"\"supports\": [integer], \"contradicts\": [integer], \"missingEvidence\": [string]}]}");

	p.user = "統合Evidence:\n";
	p.user += Truncate(builtEvidence.dump(2), 12000);
	return p;
}

PromptPair Critique(const Json& hypotheses, const Json& builtEvidence) {
	PromptPair p;
	p.system = BuildSystem(
		"仮説とEvidenceを検証するCritic担当。各観点を0〜1で採点し、欠陥と追加調査案を挙げる。"
		"ToolError、CommandValidationError、failure=trueの記録は成功Evidenceとして扱わない。"
		"現在のresolvedRequestに答えていない仮説は通過させない。",
		"{\"scores\": {\"evidenceCoverage\": number, \"contradictionHandling\": number, "
		"\"causalCompleteness\": number, \"testability\": number}, "
		"\"failures\": [string], "
		"\"additionalTasksSuggested\": [{\"type\": string, \"description\": string}]}");

	p.user = "仮説:\n";
	p.user += Truncate(hypotheses.dump(2));
	p.user += "\n\n統合Evidence:\n";
	p.user += Truncate(builtEvidence.dump(2), 12000);
	return p;
}

PromptPair Synthesize(const Json& evidence, const Json& rankedHypotheses, const Json& stopInfo) {
	PromptPair p;
	p.system = BuildSystem(
		"確定したrequestContext・Evidence・仮説・停止理由から、今回の会話に直接続くMarkdown応答を作る"
		"Synthesis担当。最新の訂正を優先し、訂正前の要求へ戻らない。"
		"新規の調査やEvidenceに基づかない断定は禁止。不確実な範囲は明記する。"
		"停止理由がcritic passedでない場合、調査完了や追加調査不要と断定しない。",
		"{\"report\": string}");

	p.user = "確定Evidenceと要求Context:\n";
	p.user += Truncate(evidence.dump(2), 14000);
	p.user += "\n\n順位付き仮説:\n";
	p.user += Truncate(rankedHypotheses.dump(2));
	p.user += "\n\n停止情報:\n";
	p.user += Truncate(stopInfo.dump(2));
	return p;
}

} // namespace prompts
} // namespace agentos
