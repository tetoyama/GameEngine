// =======================================================================
//
// PromptTemplates.cpp
//
// =======================================================================
#include "PromptTemplates.h"

#include <initializer_list>

namespace agentos {
namespace prompts {

namespace {

thread_local Json g_conversationContext = Json::object();
thread_local Json g_normalizedIntake = Json::object();

const char* kContractJa =
	"あなたはAgentOSのサブシステムとして動作するアシスタントです。\n"
	"出力契約（厳守）:\n"
	"1. 応答は単一の```json フェンスで囲まれたJSONオブジェクトのみとすること。\n"
	"2. フェンスの外に文章を書かないこと。指定されたキー以外を追加しないこと。\n"
	"3. 不確実な点は指定フィールドで表現し、存在しない事実やEvidenceを捏造しないこと。\n";

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

bool ContainsAny(const std::string& text, std::initializer_list<const char*> needles) {
	for (const char* needle : needles) {
		if (needle != nullptr && text.find(needle) != std::string::npos) return true;
	}
	return false;
}

Json IntakeWithoutRawConversation(const Json& source) {
	Json intake = source.is_object() ? source : Json::object();
	intake.erase("conversationContext");
	return intake;
}

// DB原文保存とは分離したPrompt用Context。最新Turnを必ず優先する。
std::string ContextText(const Json& supplied) {
	const Json& context = supplied.is_object() && !supplied.empty()
		? supplied
		: g_conversationContext;
	if (!context.is_object() || context.empty()) return "(会話履歴なし)";

	constexpr std::size_t kContextBudget = 12000;
	Json packed = Json::object();
	packed["summary"] = Truncate(context.value("summary", std::string()), 3500);
	packed["summarizedThroughSessionId"] =
		context.value("summarizedThroughSessionId", kInvalidId);
	packed["totalTurns"] = context.value("totalTurns", 0);
	if (context.contains("contextDegraded")) {
		packed["contextDegraded"] = context.at("contextDegraded");
	}
	if (context.contains("contextDegradedReason")) {
		packed["contextDegradedReason"] = context.at("contextDegradedReason");
	}

	Json selected = Json::array();
	std::size_t originalCount = 0;
	if (context.contains("recentTurns") && context.at("recentTurns").is_array()) {
		const Json& turns = context.at("recentTurns");
		originalCount = turns.size();
		for (std::size_t start = turns.size(); start > 0; --start) {
			Json candidate = Json::array();
			for (std::size_t i = start - 1; i < turns.size(); ++i) {
				candidate.push_back(turns[i]);
			}
			Json candidateContext = packed;
			candidateContext["recentTurns"] = candidate;
			candidateContext["omittedRecentTurnCount"] = start - 1;
			if (candidateContext.dump(2).size() <= kContextBudget) {
				selected = std::move(candidate);
				continue;
			}

			if (selected.empty() && !turns.empty() && turns.back().is_object()) {
				Json latest = turns.back();
				latest["user"] = Truncate(latest.value("user", std::string()), 3200);
				latest["assistant"] = Truncate(latest.value("assistant", std::string()), 4200);
				selected.push_back(std::move(latest));
			}
			break;
		}
	}

	packed["recentTurns"] = std::move(selected);
	packed["omittedRecentTurnCount"] =
		originalCount >= packed["recentTurns"].size()
			? originalCount - packed["recentTurns"].size()
			: 0;
	return packed.dump(2);
}

std::string RequestContextText() {
	const Json intake = IntakeWithoutRawConversation(g_normalizedIntake);
	return "解決済みIntake:\n" + Truncate(intake.dump(2), 5000) +
		"\n\n圧縮Conversation Context:\n" + ContextText(g_conversationContext);
}

} // namespace

std::string Truncate(const std::string& text, std::size_t maxChars) {
	if (text.size() <= maxChars) return text;
	return text.substr(0, maxChars) + "\n...(truncated)...";
}

void SetCurrentConversationRequestContext(
	const Json& conversationContext,
	const Json& normalizedIntake) {
	g_conversationContext = conversationContext.is_object()
		? conversationContext
		: Json::object();
	g_normalizedIntake = normalizedIntake.is_object()
		? normalizedIntake
		: Json::object();
}

void ClearCurrentConversationRequestContext() {
	g_conversationContext = Json::object();
	g_normalizedIntake = Json::object();
}

Json CurrentConversationRequestContext() {
	Json context = Json::object();
	context["conversation"] = g_conversationContext;
	context["intake"] = IntakeWithoutRawConversation(g_normalizedIntake);
	return context;
}

std::string CurrentResolvedRequest(const std::string& fallback) {
	if (g_normalizedIntake.is_object() &&
	    g_normalizedIntake.contains("resolvedRequest") &&
	    g_normalizedIntake.at("resolvedRequest").is_string() &&
	    !g_normalizedIntake.at("resolvedRequest").get<std::string>().empty()) {
		return g_normalizedIntake.at("resolvedRequest").get<std::string>();
	}
	return fallback;
}

bool CurrentRequestIsPersonalIdentityQuestion() {
	const std::string request = CurrentResolvedRequest();
	return ContainsAny(request, {
		"私は誰", "わたしは誰", "僕は誰", "自分は誰",
		"あなたは誰", "君は誰", "who am i", "Who am I",
		"who are you", "Who are you"
	});
}

std::string CompactToolCatalog(const Json& toolCatalog) {
	std::string out;
	if (!toolCatalog.is_array()) return out;

	for (const auto& tool : toolCatalog) {
		if (!tool.is_object()) continue;
		const std::string name = tool.value("name", std::string());
		const std::string description = tool.value("description", std::string());
		const std::string permission = tool.value("requiredPermission", std::string());

		std::string args;
		if (tool.contains("argumentSchema") && tool.at("argumentSchema").is_object()) {
			bool first = true;
			for (const auto& item : tool.at("argumentSchema").items()) {
				if (!first) args += ", ";
				first = false;
				const Json& spec = item.value();
				const bool required = spec.is_object() && spec.value("required", false);
				const std::string type = spec.is_object()
					? spec.value("type", std::string("any"))
					: std::string("any");
				args += item.key();
				if (!required) args += "?";
				args += ":" + type;
			}
		}
		out += "- " + name + "(" + args + ") : " + description + " [" + permission + "]\n";
	}
	return out;
}

PromptPair Intake(const std::string& userRequest, const Json& conversationContext) {
	PromptPair p;
	p.system = BuildSystem(
		"会話履歴と現在入力から、今回の要求を単独で意味の通る形へ解決するIntake担当。\n"
		"- summaryは古いTurnの累積要約、recentTurnsは最近のuser/assistant最終応答ペア。\n"
		"- 『そうじゃなくて』『違う』『それ』『続けて』『前の』『今の』等は履歴を参照する。\n"
		"- 最新の明示的な訂正・否定は、過去の矛盾する条件より必ず優先する。\n"
		"- resolvedRequestは履歴を知らない別Agentでも実行できるstandaloneな要求にする。\n"
		"- 『私/わたし/僕』『あなた』をScene Entity名へ変換しない。\n"
		"- 履歴に根拠がない参照は推測せずunresolvedReferencesへ入れる。\n"
		"requestType: investigation = Engine/Scene実データが必要。"
		"conversation = 雑談、人物質問、履歴だけで答えられる修正確認。",
		"{\"goal\": string, \"resolvedRequest\": string, "
		"\"turnRelation\": \"new\"|\"continue\"|\"correct\"|\"clarify\"|\"refer\", "
		"\"symptoms\": [string], \"constraints\": [string], "
		"\"requiredCapabilities\": [string], \"unresolvedReferences\": [string], "
		"\"requestType\": \"conversation\"|\"investigation\"}");

	p.user = "会話履歴:\n" + ContextText(conversationContext) +
		"\n\n現在のユーザー入力:\n" + userRequest +
		"\n\n履歴を反映したresolvedRequestを生成してください。";
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
		"- 人物情報や『私は誰』をScene Entity検索へ変換しない。\n"
		"- Toolはゲーム内実データを明示的に求め、1回のRead Toolで完結する場合だけ提案する。\n"
		"- 履歴だけで回答できるならreply。多段調査が必要ならescalate=true。\n"
		"- 実行していない操作を実行済みと言わない。",
		"{\"reply\": string|null, \"toolCall\": {\"tool\": string, \"arguments\": object}|null, "
		"\"escalate\": boolean}");

	p.user = "会話履歴:\n" + ContextText(conversationContext) +
		"\n\n今回の解決済み要求:\n" + CurrentResolvedRequest(userRequest) +
		"\n\n利用可能なTool一覧:\n" + compactToolCatalog;
	return p;
}

PromptPair FormatToolResult(const std::string& userRequest, const std::string& toolName, const Json& payload) {
	PromptPair p;
	p.system = BuildSystem(
		"Tool実行結果を現在の解決済み要求に沿って日本語で要約するReporter担当。"
		"結果に無い事実を足さないこと。",
		"{\"reply\": string}");
	p.user = "ユーザー要求:\n" + CurrentResolvedRequest(userRequest) +
		"\n\n実行したTool: " + toolName +
		"\n\nTool実行結果:\n" + Truncate(payload.dump(2));
	return p;
}

PromptPair CompressConversationMemory(
	const std::string& existingSummary,
	const Json& turnsToCompress) {

	PromptPair p;
	p.system = BuildSystem(
		"古いConversation Turnを累積会話要約へ圧縮するMemory担当。\n"
		"- 各Turnはuser入力とassistant最終応答のペアとして読む。\n"
		"- 目的、確定仕様、好み、対象名、訂正、未完了事項を保持する。\n"
		"- 後の訂正が矛盾する場合、最新状態を正文とする。\n"
		"- Tool中間ログ、思考過程、冗長な言い回しは残さない。\n"
		"- 事実を追加しない。summaryは4000文字程度以内。",
		"{\"summary\": string}");
	p.user = "既存の累積要約:\n" +
		(existingSummary.empty() ? std::string("(なし)") : Truncate(existingSummary, 6000)) +
		"\n\n今回要約へ畳み込むTurn:\n" + Truncate(turnsToCompress.dump(2), 14000);
	return p;
}

PromptPair Plan(const Json& intake, const Json& toolCatalog, int maxTasks) {
	PromptPair p;
	p.system = BuildSystem(
		"Intake結果とTool一覧からTask DAGを作るPlanner担当。task数は" +
		std::to_string(maxTasks) + "件以内。\n"
		"- resolvedRequestと最新constraintsを正とし、訂正前の条件を復活させない。\n"
		"- snapshot観測ではListEntities/ListSystems/DescribeEntity等を使い、"
		"時間変化が明示されない限りWriteTraceを使わない。\n"
		"- Toolを実行するTaskはAnalysisにしない。\n"
		"- Entity名等は先行Taskで取得しdependenciesでGroundingする。",
		"{\"tasks\": [{\"taskId\": string, "
		"\"type\": \"RuntimeObservation\"|\"CodeSearch\"|\"Trace\"|\"Analysis\", "
		"\"description\": string, \"dependencies\": [string], "
		"\"allowedTools\": [string], \"searchHints\": [string]}]}");

	const Json planningIntake = IntakeWithoutRawConversation(intake);
	p.user = "解決済みIntake:\n" + Truncate(planningIntake.dump(2), 7000) +
		"\n\nTool一覧:\n" + Truncate(CompactToolCatalog(toolCatalog)) +
		"\n\n最大Task数: " + std::to_string(maxTasks);
	return p;
}

PromptPair GenerateQueries(const Json& taskSpec, const Json& toolCatalog) {
	PromptPair p;
	p.system = BuildSystem(
		"Task遂行用Tool呼び出しを最大5件提案するWorker担当。\n"
		"- dependencyEvidenceのEntity名・Component名等は実在文字列を完全一致でコピー。\n"
		"- 空文字、主要なEntity、対象Component、TODO等は禁止。\n"
		"- 必要値をEvidenceから決められない場合はcommandsを空配列にする。\n"
		"- allowedTools外は禁止。",
		"{\"commands\": [{\"tool\": string, \"arguments\": object}]}");
	p.user = "Task spec:\n" + Truncate(taskSpec.dump(2), 10000) +
		"\n\nTool一覧:\n" + Truncate(CompactToolCatalog(toolCatalog));
	return p;
}

PromptPair Reason(const Json& builtEvidence) {
	PromptPair p;
	p.system = BuildSystem(
		"解決済み要求と統合Evidenceのみから仮説を組み立てるReasoning担当。"
		"最新のresolvedRequest/constraintsを優先し、Evidenceに無い事実は書かない。",
		"{\"hypotheses\": [{\"description\": string, \"rubricBase\": number, "
		"\"supports\": [integer], \"contradicts\": [integer], \"missingEvidence\": [string]}]}");
	p.user = RequestContextText() +
		"\n\n統合Evidence:\n" + Truncate(builtEvidence.dump(2), 10000);
	return p;
}

PromptPair Critique(const Json& hypotheses, const Json& builtEvidence) {
	PromptPair p;
	p.system = BuildSystem(
		"解決済み要求に対して仮説とEvidenceを検証するCritic担当。"
		"ToolError等は成功Evidenceとして扱わず、要求に答えていない仮説は通過させない。",
		"{\"scores\": {\"evidenceCoverage\": number, \"contradictionHandling\": number, "
		"\"causalCompleteness\": number, \"testability\": number}, "
		"\"failures\": [string], "
		"\"additionalTasksSuggested\": [{\"type\": string, \"description\": string}]}");
	p.user = RequestContextText() +
		"\n\n仮説:\n" + Truncate(hypotheses.dump(2), 5000) +
		"\n\n統合Evidence:\n" + Truncate(builtEvidence.dump(2), 9000);
	return p;
}

PromptPair Synthesize(const Json& evidence, const Json& rankedHypotheses, const Json& stopInfo) {
	PromptPair p;
	p.system = BuildSystem(
		"解決済み要求・会話Context・Evidence・仮説・停止理由から、今回の会話に直接続く応答を作る。"
		"最新の訂正を優先し、訂正前へ戻らない。Evidence外の断定は禁止。"
		"critic passedでない場合、調査完了と断定しない。",
		"{\"report\": string}");
	p.user = RequestContextText() +
		"\n\n確定Evidence:\n" + Truncate(evidence.dump(2), 10000) +
		"\n\n順位付き仮説:\n" + Truncate(rankedHypotheses.dump(2), 5000) +
		"\n\n停止情報:\n" + Truncate(stopInfo.dump(2), 2000);
	return p;
}

} // namespace prompts
} // namespace agentos
