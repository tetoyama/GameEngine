// =======================================================================
//
// AgentOSConversationHistoryToolSmokeTest.cpp
//
// 実機失敗（transcript_20260727_013726）:
//   セッション75が「呼び出し箇所」を5件返した直後、
//   ユーザが「5件全部知りたい」と入力した。
//   IntakeへのContextは totalTurns=0 / threadStates=[] で、
//   DBに51件の履歴があるのに1件も渡っていなかった。
//   Intakeは情報ゼロの中で turnRelation=clarify・unresolvedReferences付きと
//   正しく申告したが、パイプラインはそのまま調査へ突入し、
//   修復2ラウンドを空振りして「未完了」で終わった。
//
//   原因は IntakeAgent::Run が履歴をDBから読むかどうかを
//   RequiresPriorThreadContext（14語の部分一致）だけで決めていたこと。
//   「5件全部知りたい」はどの語にも当たらない。
//
// 対策は「要るかを先回りで決める」のをやめ、要ると気づいた側が
// 取りに行けるようにすること（GetConversationHistoryツール）。
//
// あわせて、履歴を今回のEvidenceとして載せる以上、
// 過去のAgent応答が今回の断定の根拠になってはいけない。
// これはプロンプトではなくCriticのゲート#7で決定的に止める。
//
// =======================================================================
#include "AgentOS/Core/Agents/AgentContext.h"
#include "AgentOS/Core/Agents/CriticAgent.h"
#include "AgentOS/Core/Command/CapabilitySet.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Conversation/ConversationHistoryTool.h"
#include "AgentOS/Core/Json.h"
#include "AgentOS/Core/Llm/PromptTemplates.h"
#include "AgentOS/Core/Store/TaskStore.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace agentos;

namespace {

const char* kDbPath = "/tmp/agentos_conversation_history_tool.db";

CommandRequest MakeRequest(
	const ToolName& tool, const Json& arguments, const CapabilityToken& token) {
	CommandRequest request;
	request.issuer = token.owner;
	request.tool = tool;
	request.arguments = arguments;
	request.capability = token;
	return request;
}

// ---------------------------------
// 過去2セッション分の記録を仕込む。
//   session 1: 検索を実行してEvidenceを残した（＝観測）
//   session 2: その結果を要約して応答した（＝推測）
// ---------------------------------
void SeedHistory(TaskStore& store) {
	const SessionId s1 = store.CreateSession(Json::object({{"goal", "呼び出し箇所を調べる"}}));
	assert(s1 != kInvalidId);
	const TaskId t1 = store.CreateTask(s1, kInvalidId, "CodeSearch", Json::object(), 0);

	Evidence found;
	found.taskId = t1;
	found.claim = "「呼び出し処理」に一致するコードを5件見つけた。";
	found.payload = Json::object({
		{"count", 5},
		{"results", Json::array({
			"agentos::IntakeAgent", "agentos::TaskScheduler", "agentos::LlmCallLogger",
			"agentos::CriticAgent", "agentos::SqliteDb::Bind",
		})},
	});
	found.provenance.sourceType = "Tool:FindCodeReferences";
	found.provenance.sourceUri = "FindCodeReferences";
	found.provenance.session = "session_1";
	assert(store.AddEvidence(found) != kInvalidId);

	store.SetConversationResponse(s1, "「呼び出し処理」に関連するコードを5件発見しました。");
	store.SetConversationThreadState(s1, Json::object({
		{"goal", "呼び出し箇所を調べる"}, {"unresolved", Json::array()},
	}));

	const SessionId s2 = store.CreateSession(Json::object({{"goal", "無関係な別件"}}));
	assert(s2 != kInvalidId);
	store.SetConversationResponse(s2, "別件の応答です。");
}

// ---------------------------------
// 1) Toolが4種別を横断して返すこと
// ---------------------------------
void TestToolReturnsAllKinds() {
	std::remove(kDbPath);
	TaskStore store;
	assert(store.Open(kDbPath));
	SeedHistory(store);

	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	RegisterConversationHistoryTool(pipeline, &store);
	const CapabilityToken token =
		registry.IssueToken("test", {"*"}, PermissionLevel::Read);

	// Toolは現在セッションIDを境界に使う。未設定なら弾かれること。
	SetCurrentSessionId(kInvalidId);
	assert(pipeline.Submit(
		MakeRequest("GetConversationHistory", Json::object(), token)).status != CommandStatus::Ok);

	SetCurrentSessionId(99);
	const CommandResult result =
		pipeline.Submit(MakeRequest("GetConversationHistory", Json::object(), token));
	assert(result.status == CommandStatus::Ok);

	const Json& payload = result.payload;
	assert(payload.contains("entries") && payload.at("entries").is_array());

	bool sawEvidence = false, sawUser = false, sawThreadState = false, sawAssistant = false;
	for (const Json& entry : payload.at("entries")) {
		const std::string kind = entry.value("kind", std::string());
		if (kind == "evidence") {
			sawEvidence = true;
			// 過去のTool実行結果は観測。断定の根拠になれる。
			assert(entry.value("role", std::string()) == "observation");
		} else if (kind == "userTurn") {
			sawUser = true;
			assert(entry.value("role", std::string()) == "reference");
		} else if (kind == "threadState") {
			sawThreadState = true;
			assert(entry.value("role", std::string()) == "reference");
		} else if (kind == "assistantTurn") {
			sawAssistant = true;
			assert(entry.value("role", std::string()) == "reference");
		}
	}
	assert(sawEvidence);
	assert(sawUser);
	assert(sawThreadState);
	assert(sawAssistant);
	assert(payload.value("observationCount", std::size_t(0)) >= 1);

	// 「5件」の実体（過去Evidenceのpayload）が引けていること。
	// 実機ではここが取れず、Intakeが別セッションのEntity一覧を掴んでいた。
	assert(result.payload.dump().find("agentos::IntakeAgent") != std::string::npos);

	SetCurrentSessionId(0);
	std::remove(kDbPath);
	std::printf("  [ok] GetConversationHistory returns evidence/userTurn/threadState/assistantTurn\n");
}

// ---------------------------------
// 2) queryで絞れること / 種別を選べること
// ---------------------------------
void TestQueryAndKindFilter() {
	std::remove(kDbPath);
	TaskStore store;
	assert(store.Open(kDbPath));
	SeedHistory(store);

	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	RegisterConversationHistoryTool(pipeline, &store);
	const CapabilityToken token =
		registry.IssueToken("test", {"*"}, PermissionLevel::Read);
	SetCurrentSessionId(99);

	const CommandResult result = pipeline.Submit(MakeRequest(
		"GetConversationHistory",
		Json::object({{"query", "呼び出し処理"}, {"kinds", Json::array({"evidence"})}}),
		token));
	assert(result.status == CommandStatus::Ok);

	assert(!result.payload.at("entries").empty());
	for (const Json& entry : result.payload.at("entries")) {
		assert(entry.value("kind", std::string()) == "evidence");
	}

	// 未知の種別は実行前に弾く。
	assert(pipeline.Submit(MakeRequest(
		"GetConversationHistory",
		Json::object({{"kinds", Json::array({"nonsense"})}}),
		token)).status != CommandStatus::Ok);

	SetCurrentSessionId(0);
	std::remove(kDbPath);
	std::printf("  [ok] query / kinds filter works; unknown kind is rejected\n");
}

// ---------------------------------
// 3) ゲート#7: 参照系Evidenceだけでは仮説を支えられないこと
// ---------------------------------
Json HistoryEvidence(std::int64_t id, std::size_t observationCount, const char* role) {
	return Json::object({
		{"id", id},
		{"taskId", 1},
		{"claim", "過去のやり取りから記録を取得した。"},
		{"confidence", 1.0},
		{"provenance", Json::object({
			{"sourceType", "Tool:GetConversationHistory"},
			{"sourceUri", "GetConversationHistory"},
			{"session", "s1"}, {"frame", -1},
		})},
		{"payload", Json::object({
			{"count", 1},
			{"observationCount", observationCount},
			{"entries", Json::array({Json::object({
				{"kind", observationCount > 0 ? "evidence" : "assistantTurn"},
				{"role", role},
				{"sessionId", 1},
			})})},
		})},
	});
}

bool HasFailure(const CriticVerdict& verdict, const char* needle) {
	for (const auto& failure : verdict.failures) {
		if (failure.find(needle) != std::string::npos) return true;
	}
	return false;
}

void RunCritic(const Json& evidences, CriticVerdict* out) {
	prompts::ClearCurrentConversationRequestContext();
	prompts::SetCurrentConversationRequestContext(
		Json::object(),
		Json::object({
			{"currentUserInput", "5件全部知りたい"},
			{"resolvedRequest", "直前に提示された5件の詳細を示す"},
		}));

	const Json ranked = Json::object({{"hypotheses", Json::array({Json::object({
		{"id", 1},
		{"text", "5件とは呼び出し箇所の一覧である"},
		{"confidence", 0.9},
		{"supports", Json::array({1})},
		{"contradicts", Json::array()},
		{"missingEvidence", Json::array()},
	})})}});

	const Json built = Json::object({
		{"coverage", 1.0},
		{"tasksWithoutEvidence", Json::array()},
		{"failedEvidenceCount", 0},
		{"usableEvidenceCount", evidences.size()},
		{"contradictions", Json::array()},
		{"evidences", evidences},
	});

	AgentContext ctx; // llmはnullptr。LLM所見はadvisoryなので決定的ゲートは効く。
	CriticAgent::Run(ctx, ranked, built, out);
	prompts::ClearCurrentConversationRequestContext();
}

void TestReferenceOnlySupportIsRejected() {
	// 3a: 過去のAgent応答だけを根拠にした仮説は落とす。
	{
		CriticVerdict verdict;
		RunCritic(Json::array({HistoryEvidence(1, 0, "reference")}), &verdict);
		assert(HasFailure(verdict, "supported only by conversation references"));
		assert(!verdict.pass);
	}

	// 3b: 過去のTool実行結果（観測）が含まれていれば、この理由では落とさない。
	{
		CriticVerdict verdict;
		RunCritic(Json::array({HistoryEvidence(1, 1, "observation")}), &verdict);
		assert(!HasFailure(verdict, "supported only by conversation references"));
	}

	// 3c: 通常のEngine Tool Evidenceは当然観測として扱う（回帰確認）。
	{
		CriticVerdict verdict;
		RunCritic(Json::array({Json::object({
			{"id", 1}, {"taskId", 1}, {"confidence", 1.0},
			{"claim", "Entityは5件"},
			{"provenance", Json::object({
				{"sourceType", "Tool:ListEntities"}, {"sourceUri", "ListEntities"},
				{"session", "s1"}, {"frame", -1},
			})},
			{"payload", Json::object({{"count", 5}})},
		})}), &verdict);
		assert(!HasFailure(verdict, "supported only by conversation references"));
	}

	std::printf("  [ok] gate#7 rejects hypotheses supported only by conversation references\n");
}

// ---------------------------------
// 4) 最終応答へ反映されなかった失敗Evidenceは引き継がないこと
// ---------------------------------
// 実機（transcript_20260727_155429）:
//   「今日はいい天気ですね」に対しIntakeが前ターンの名前「Taro」を
//   resolvedEntityName へ引き継ぎ、修復ラウンドが存在しないTaroを探して
//   ToolUnsatisfied を3件作った。応答にはその失敗は反映されていない。
//   この記録が次セッションへ「観測」として戻ると、同じ探索を繰り返させる。
//
// 反映された失敗（例:「そのEntityは存在しなかった」と答えた）は残す。
// 「前回これは無かった」という知識であり、同じ轍を防ぐ側に働くため。
void TestUnreflectedFailureIsNotCarriedOver() {
	std::remove(kDbPath);
	TaskStore store;
	assert(store.Open(kDbPath));

	const SessionId session = store.CreateSession(Json::object({{"goal", "挨拶へ応答する"}}));
	const TaskId task = store.CreateTask(session, kInvalidId, "Retrieval", Json::object(), 0);

	Evidence unreflected;
	unreflected.taskId = task;
	unreflected.claim = "UNREFLECTED_FAILURE: Entity 'Taro' は見つからなかった";
	unreflected.provenance.sourceType = "ToolUnsatisfied";
	unreflected.provenance.sourceUri = "FindEntityByName";
	const EvidenceId unreflectedId = store.AddEvidence(unreflected);
	assert(unreflectedId != kInvalidId);

	Evidence reflected;
	reflected.taskId = task;
	reflected.claim = "REFLECTED_FAILURE: Component 'JumpForce' は存在しなかった";
	reflected.provenance.sourceType = "ToolUnsatisfied";
	reflected.provenance.sourceUri = "ReadComponent";
	const EvidenceId reflectedId = store.AddEvidence(reflected);
	assert(reflectedId != kInvalidId);

	Evidence success;
	success.taskId = task;
	success.claim = "SUCCESS_EVIDENCE: Entityを5件取得した";
	success.provenance.sourceType = "Tool:ListEntities";
	success.provenance.sourceUri = "ListEntities";
	assert(store.AddEvidence(success) != kInvalidId);

	// 最終仮説が根拠に挙げたのは reflected だけ、という状況を作る。
	assert(store.MarkEvidenceReflected({reflectedId}));

	store.SetConversationResponse(session, "こんにちは。");

	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	RegisterConversationHistoryTool(pipeline, &store);
	const CapabilityToken token = registry.IssueToken("test", {"*"}, PermissionLevel::Read);
	SetCurrentSessionId(99);

	const CommandResult result = pipeline.Submit(
		MakeRequest("GetConversationHistory", Json::object({{"limit", 20}}), token));
	assert(result.status == CommandStatus::Ok);
	const std::string dumped = result.payload.dump();

	assert(dumped.find("UNREFLECTED_FAILURE") == std::string::npos);
	assert(dumped.find("REFLECTED_FAILURE") != std::string::npos);
	assert(dumped.find("SUCCESS_EVIDENCE") != std::string::npos);

	SetCurrentSessionId(0);
	std::remove(kDbPath);
	std::printf("  [ok] unreflected failure evidence is not carried into a new session\n");
}

} // namespace

int main() {
	std::printf("=== AgentOS Conversation History Tool Smoke Test ===\n");

	TestToolReturnsAllKinds();
	TestQueryAndKindFilter();
	TestReferenceOnlySupportIsRejected();
	TestUnreflectedFailureIsNotCarriedOver();

	std::printf("=== ALL PASSED ===\n");
	return 0;
}
