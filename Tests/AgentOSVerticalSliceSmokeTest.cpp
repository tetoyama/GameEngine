// =======================================================================
//
// AgentOSVerticalSliceSmokeTest.cpp
//
// AgentOS Core 垂直スライスのE2Eスモークテスト（構想§14 / 01_Phase_Plan.md）。
// ユースケース: 「特定EntityのComponent値が異常な理由を調査し、原因Systemを
// 特定する」を、MockLlmBackend（台本方式）＋FakeEngine Tool（インメモリWorld）
// で最初から最後まで通す。エンジン規約どおり自己完結main()+assert方式。
//
// =======================================================================
#include "AgentOS/Core/AgentOsTypes.h"
#include "AgentOS/Core/Json.h"
#include "AgentOS/Core/Command/CapabilitySet.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Command/CommandTypes.h"
#include "AgentOS/Core/Llm/MockLlmBackend.h"
#include "AgentOS/Core/Orchestrator/Orchestrator.h"
#include "AgentOS/Core/Store/TaskStore.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace agentos;

namespace {

// ---------------------------------
// FakeEngine Tools
// ハードコードされたインメモリWorldに対するICommandExecutor実装。
// ---------------------------------

class DescribeEntityTool : public ICommandExecutor {
public:
	DescribeEntityTool() {
		descriptor_.name = "DescribeEntity";
		descriptor_.description = "指定Entityの現在のComponent値を返す（テスト用Fake）";
		descriptor_.requiredPermission = PermissionLevel::Read;
		Json schema = Json::object();
		schema["entityName"] = Json::object({{"type", "string"}, {"required", true}});
		descriptor_.argumentSchema = schema;
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }

	CommandResult Execute(const Json& arguments) override {
		const std::string entityName = arguments.value("entityName", std::string());
		if (entityName != "Boss") {
			return CommandResult::Fail(CommandStatus::ExecutionFailed, "unknown entity: " + entityName);
		}
		Json payload = Json::object();
		payload["claim"] = "Boss entity snapshot: Velocity.y=8.2 while state=Idle";
		Json components = Json::object();
		components["BossState"] = Json::object({{"state", "Idle"}});
		components["Velocity"] = Json::object({{"y", 8.2}});
		components["Transform"] = Json::object({{"position", Json::array({2.3, 5.1, -4.0})}});
		payload["components"] = components;
		return CommandResult::Ok(payload);
	}

private:
	ToolDescriptor descriptor_;
};

class GetWriteTraceTool : public ICommandExecutor {
public:
	GetWriteTraceTool() {
		descriptor_.name = "GetWriteTrace";
		descriptor_.description = "指定Componentへの書込トレースを返す（テスト用Fake）";
		descriptor_.requiredPermission = PermissionLevel::Observe;
		Json schema = Json::object();
		schema["entityName"] = Json::object({{"type", "string"}, {"required", true}});
		schema["component"] = Json::object({{"type", "string"}, {"required", true}});
		descriptor_.argumentSchema = schema;
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }

	CommandResult Execute(const Json& arguments) override {
		const std::string component = arguments.value("component", std::string());
		if (component != "Velocity") {
			return CommandResult::Fail(CommandStatus::ExecutionFailed, "no trace for component: " + component);
		}
		Json payload = Json::object();
		payload["claim"] = "BossJumpAttackSystem wrote Velocity.y=8.2 at frame 18420; no reset write afterwards";
		Json records = Json::array();
		records.push_back(Json::object({
			{"frame", 18420}, {"system", "BossJumpAttackSystem"}, {"field", "y"}, {"before", 0.0}, {"after", 8.2}}));
		records.push_back(Json::object({{"frame", 18422}, {"note", "no write after state transition"}}));
		payload["records"] = records;
		return CommandResult::Ok(payload);
	}

private:
	ToolDescriptor descriptor_;
};

class FindWritersTool : public ICommandExecutor {
public:
	FindWritersTool() {
		descriptor_.name = "FindWriters";
		descriptor_.description = "指定Componentへの書込System一覧を返す（テスト用Fake）";
		descriptor_.requiredPermission = PermissionLevel::Read;
		Json schema = Json::object();
		schema["component"] = Json::object({{"type", "string"}, {"required", true}});
		descriptor_.argumentSchema = schema;
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }

	CommandResult Execute(const Json& arguments) override {
		const std::string component = arguments.value("component", std::string());
		Json payload = Json::object();
		if (component == "Velocity") {
			payload["claim"] = "Writers of Velocity: BossJumpAttackSystem, PhysicsSystem";
			payload["writers"] = Json::array({"BossJumpAttackSystem", "PhysicsSystem"});
		} else {
			payload["claim"] = "Writers of " + component + ": (none known)";
			payload["writers"] = Json::array();
		}
		return CommandResult::Ok(payload);
	}

private:
	ToolDescriptor descriptor_;
};

// Modify権限のFake Tool。どのTaskのallowedToolsにも含めない
// （Capability違反テスト専用。Workerからは決して呼ばれない）。
class SetComponentFieldTool : public ICommandExecutor {
public:
	SetComponentFieldTool() {
		descriptor_.name = "SetComponentField";
		descriptor_.description = "Component値を書き換える（テスト用Fake, Modify権限必須）";
		descriptor_.requiredPermission = PermissionLevel::Modify;
		Json schema = Json::object();
		schema["entityName"] = Json::object({{"type", "string"}, {"required", true}});
		schema["field"] = Json::object({{"type", "string"}, {"required", true}});
		schema["value"] = Json::object({{"type", "number"}, {"required", true}});
		descriptor_.argumentSchema = schema;
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }

	CommandResult Execute(const Json&) override {
		return CommandResult::Ok(Json::object({{"applied", true}}));
	}

private:
	ToolDescriptor descriptor_;
};

// クイックToolパス検証用のFake Tool。Read権限、引数無しでシーンEntity一覧を返す。
class ListEntitiesTool : public ICommandExecutor {
public:
	ListEntitiesTool() {
		descriptor_.name = "ListEntities";
		descriptor_.description = "シーン中のEntity一覧を返す（テスト用Fake）";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object(); // 引数無し
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }

	CommandResult Execute(const Json&) override {
		Json payload = Json::object();
		payload["claim"] = "生存Entityは2件";
		payload["entities"] = Json::array({
			Json::object({{"name", "Boss"}}),
			Json::object({{"name", "Player"}})});
		return CommandResult::Ok(payload);
	}

private:
	ToolDescriptor descriptor_;
};

void RemoveDb(const std::string& path) {
	std::remove(path.c_str());
	std::remove((path + "-wal").c_str());
	std::remove((path + "-shm").c_str());
	std::remove((path + "-journal").c_str());
}

// MockLlmBackendのAddRuleは、systemPrompt+userPromptの部分文字列一致で判定される。
// 注意: 各段のuserPromptには前段の出力JSONがdumpされて埋め込まれる
// （例: Plan呼び出しのuserPromptにはIntakeの出力JSONがまるごと含まれる）。
// そのため出力スキーマのキー名（例: "requiredCapabilities"）を目印にすると、
// 後続の呼び出しへ意図せず伝播してマッチしてしまう。
// 各PromptTemplates関数のsystemPrompt冒頭にある「役割」記述（例: "Intake担当"）は
// その段のsystemPromptにしか現れず、後続段のuserPromptに埋め込まれる出力JSONには
// 含まれないため、これを目印として使う。
// GenerateQueriesだけはT1/T2で同一システムプロンプトを使うため、taskSpecの
// dump(2)に含まれる "taskId": "T1" / "taskId": "T2" をユーザープロンプト側の
// 目印にする（taskId文字列は他段のuserPromptへは伝播しない）。
void SetupHappyPathMockLlm(MockLlmBackend& llm) {
	// Intake: 役割記述中の "Intake担当" が目印
	llm.AddRule("Intake担当",
		"```json\n"
		"{\"goal\": \"Bossが Idle 状態なのに Velocity.y が異常な値になっている原因Systemを特定する\", "
		"\"symptoms\": [\"Boss.Velocity.y が Idle 中に 8.2 になっている\"], \"constraints\": [], "
		"\"requiredCapabilities\": [\"ReadComponent\", \"WriteTrace\"]}\n"
		"```");

	// Plan: 役割記述中の "Planner担当" が目印
	llm.AddRule("Planner担当",
		"```json\n"
		"{\"tasks\": ["
		"{\"taskId\": \"T1\", \"type\": \"RuntimeObservation\", "
		"\"description\": \"Boss entityの現況とVelocity書込元候補を調べる\", "
		"\"dependencies\": [], \"allowedTools\": [\"DescribeEntity\", \"FindWriters\"], "
		"\"searchHints\": [\"Boss\", \"Velocity\"]}, "
		"{\"taskId\": \"T2\", \"type\": \"Trace\", "
		"\"description\": \"Velocityの書込トレースを取得する\", "
		"\"dependencies\": [\"T1\"], \"allowedTools\": [\"GetWriteTrace\"], "
		"\"searchHints\": [\"Velocity\"]}"
		"]}\n"
		"```");

	// GenerateQueries(T1): taskSpec.dump(2)中の "taskId": "T1" が目印
	llm.AddRule("\"taskId\": \"T1\"",
		"```json\n"
		"{\"commands\": ["
		"{\"tool\": \"DescribeEntity\", \"arguments\": {\"entityName\": \"Boss\"}}, "
		"{\"tool\": \"FindWriters\", \"arguments\": {\"component\": \"Velocity\"}}"
		"]}\n"
		"```");

	// GenerateQueries(T2): taskSpec.dump(2)中の "taskId": "T2" が目印
	llm.AddRule("\"taskId\": \"T2\"",
		"```json\n"
		"{\"commands\": ["
		"{\"tool\": \"GetWriteTrace\", \"arguments\": {\"entityName\": \"Boss\", \"component\": \"Velocity\"}}"
		"]}\n"
		"```");

	// Reason: 役割記述中の "Reasoning担当" が目印。Evidence id 1..3を根拠とする。
	llm.AddRule("Reasoning担当",
		"```json\n"
		"{\"hypotheses\": ["
		"{\"description\": \"JumpAttack終了時にVelocity.yがリセットされていない\", \"rubricBase\": 0.9, "
		"\"supports\": [1, 2, 3], \"contradicts\": [], \"missingEvidence\": []}"
		"]}\n"
		"```");

	// Critique: 役割記述中の "Critic担当" が目印
	llm.AddRule("Critic担当",
		"```json\n"
		"{\"scores\": {\"evidenceCoverage\": 0.9, \"contradictionHandling\": 1.0, "
		"\"causalCompleteness\": 0.8, \"testability\": 0.7}, "
		"\"failures\": [], \"additionalTasksSuggested\": []}\n"
		"```");

	// Synthesize: 役割記述中の "Synthesis担当" が目印
	llm.AddRule("Synthesis担当",
		"```json\n"
		"{\"report\": \"## 調査結果\\n\\nBossJumpAttackSystem がJumpAttack中にVelocity.y=8.2を書き込んだ後、"
		"Idle状態へ遷移してもリセットする処理が存在しないため、Idle中もVelocity.yが残存していると考えられます。\"}\n"
		"```");
}

} // namespace

int main() {
	const std::string dbPath = "/tmp/agentos_e2e.db";

	// ============================================================
	// シナリオ1: ハッピーパス
	// 原因System特定が完了し、報告がBossJumpAttackSystemに言及すること。
	// ============================================================
	{
		RemoveDb(dbPath);

		TaskStore store;
		Result openResult = store.Open(dbPath);
		assert(openResult.ok);

		CapabilityRegistry registry;
		CommandPipeline pipeline(&registry);
		pipeline.RegisterTool(std::make_shared<DescribeEntityTool>());
		pipeline.RegisterTool(std::make_shared<GetWriteTraceTool>());
		pipeline.RegisterTool(std::make_shared<FindWritersTool>());
		pipeline.RegisterTool(std::make_shared<SetComponentFieldTool>()); // Modify、Workerの許可リストには含めない

		MockLlmBackend llm;
		SetupHappyPathMockLlm(llm);

		OrchestratorConfig config;
		config.maxRepairRounds = 2;

		Orchestrator orchestrator(&llm, &pipeline, &store, &registry, config);

		std::vector<std::string> stages;
		orchestrator.SetProgressCallback([&](const std::string& stage, const Json& detail) {
			(void)detail;
			stages.push_back(stage);
		});

		const OrchestratorResult result = orchestrator.RunSession(
			"Bossが Idle 状態なのに Velocity.y が 8.2 のままで、想定外に落下しているように見える。"
			"原因Systemを特定してほしい。");

		assert(result.sessionId != kInvalidId);
		assert(result.completed);
		assert(!result.report.empty());
		assert(result.report.find("BossJumpAttackSystem") != std::string::npos);

		assert(result.rankedHypotheses.contains("hypotheses"));
		assert(!result.rankedHypotheses.at("hypotheses").empty());
		const double topConfidence = result.rankedHypotheses.at("hypotheses")[0].value("confidence", 0.0);
		assert(topConfidence > 0.5);

		// 主要ステージが一通り進捗通知されたこと
		auto sawStage = [&](const std::string& s) {
			for (const auto& st : stages) {
				if (st == s) return true;
			}
			return false;
		};
		assert(sawStage("Intake"));
		assert(sawStage("Plan"));
		assert(sawStage("Retrieve"));
		assert(sawStage("Reason"));
		assert(sawStage("Critic"));
		assert(sawStage("Synthesize"));

		// --- Store検証: session / >=3 tasks / >=3 evidences ---
		const Json summary = store.GetSessionSummary(result.sessionId);
		std::int64_t taskTotal = 0;
		for (const auto& item : summary.at("tasksByState").items()) {
			taskTotal += item.value().get<std::int64_t>();
		}
		assert(taskTotal >= 3); // Intake(root) + T1 + T2
		assert(summary.at("evidenceCount").get<std::int64_t>() >= 3);
		assert(summary.at("commandCount").get<std::int64_t>() >= 3);

		const std::vector<Evidence> evidences = store.GetEvidenceForSession(result.sessionId);
		assert(evidences.size() >= 3);

		// --- >=3 command監査行がexecution_status "Ok" であること ---
		// TaskStoreAuditSink（Orchestrator.cpp）はCommandPipelineの全Submit結果を
		// ToString(status)のまま validation_status/execution_status としてTaskStore
		// へ永続化する（1:1のpassthrough）。よってpipeline側の内蔵監査ログで
		// status==Okの件数を数えることは、Store側のexecution_status=="Ok"行数の
		// 直接的な検証になる。
		const auto auditLog = pipeline.GetAuditLog();
		int okCount = 0;
		for (const auto& entry : auditLog) {
			if (entry.second.status == CommandStatus::Ok) {
				++okCount;
			}
		}
		assert(okCount >= 3);

		// --- Capability違反テスト ---
		// Modify権限のSetComponentFieldはどのTaskのallowedToolsにも含まれておらず、
		// かつOrchestratorが発行したトークンはmaxPermission=Observe止まりなので、
		// 直接Submitしても必ずCapabilityRejectedになる（Modifyは絶対に許可されない）。
		const CapabilityToken issuedToken = orchestrator.GetLastIssuedToken();
		CommandRequest modifyRequest;
		modifyRequest.issuer = issuedToken.owner;
		modifyRequest.tool = "SetComponentField";
		modifyRequest.arguments = Json::object({{"entityName", "Boss"}, {"field", "y"}, {"value", 0.0}});
		modifyRequest.capability = issuedToken;
		const CommandResult modifyResult = pipeline.Submit(modifyRequest);
		assert(modifyResult.status == CommandStatus::CapabilityRejected);

		std::cout << "  - Scenario 1 (happy path, root-cause identified): OK" << std::endl;
	}

	// ============================================================
	// シナリオ2: EarlyStopping / Repair枯渇
	// Criticが常にpass=false・追加調査案なしを返す → orchestratorは
	// stopInfo.reasonに "repair rounds exhausted" か "early stopping" を残して
	// 停止し、それでもフォールバック報告（非空）を返すこと。
	// ============================================================
	{
		RemoveDb(dbPath);

		TaskStore store;
		Result openResult = store.Open(dbPath);
		assert(openResult.ok);

		CapabilityRegistry registry;
		CommandPipeline pipeline(&registry);
		pipeline.RegisterTool(std::make_shared<DescribeEntityTool>());
		pipeline.RegisterTool(std::make_shared<GetWriteTraceTool>());
		pipeline.RegisterTool(std::make_shared<FindWritersTool>());

		MockLlmBackend llm;
		// 各ルールは役割記述（PromptTemplates.cppのBuildSystem引数）中のユニークな
		// フレーズを目印にする（schemaキー名は後続段のuserPromptへ伝播するため使わない）。
		llm.AddRule("Intake担当",
			"```json\n{\"goal\": \"原因不明の異常挙動を調べる\", \"symptoms\": [], \"constraints\": [], "
			"\"requiredCapabilities\": []}\n```");
		llm.AddRule("Planner担当",
			"```json\n{\"tasks\": [{\"taskId\": \"T1\", \"type\": \"RuntimeObservation\", "
			"\"description\": \"Bossの状態を調べる\", \"dependencies\": [], "
			"\"allowedTools\": [\"DescribeEntity\"], \"searchHints\": [\"Boss\"]}]}\n```");
		llm.AddRule("\"taskId\": \"T1\"",
			"```json\n{\"commands\": [{\"tool\": \"DescribeEntity\", \"arguments\": {\"entityName\": \"Boss\"}}]}\n```");
		// Reasoning: 支持Evidence無しの弱い仮説のみ → confidence常に0
		llm.AddRule("Reasoning担当",
			"```json\n{\"hypotheses\": [{\"description\": \"原因不明\", \"rubricBase\": 0.5, "
			"\"supports\": [], \"contradicts\": [], \"missingEvidence\": [\"追加のTraceが必要\"]}]}\n```");
		// Critique: 常にpass不可、追加調査案も無し
		llm.AddRule("Critic担当",
			"```json\n{\"scores\": {\"evidenceCoverage\": 0.1, \"contradictionHandling\": 0.1, "
			"\"causalCompleteness\": 0.1, \"testability\": 0.1}, \"failures\": [\"evidence不足\"], "
			"\"additionalTasksSuggested\": []}\n```");
		// Synthesizeのルールはあえて登録しない → 決定的フォールバック報告を確認する

		OrchestratorConfig config;
		config.maxRepairRounds = 2;
		Orchestrator orchestrator(&llm, &pipeline, &store, &registry, config);

		const OrchestratorResult result = orchestrator.RunSession("原因不明の異常挙動を調べてほしい。");

		assert(!result.completed); // topHypothesis confidence が常に0 (<0.4) なのでpassしない
		assert(!result.report.empty());
		assert(result.stopInfo.contains("reason"));
		const std::string reason = result.stopInfo.value("reason", std::string());
		const bool mentionsRepairExhausted = reason.find("repair rounds exhausted") != std::string::npos;
		const bool mentionsEarlyStopping = reason.find("early stopping") != std::string::npos;
		// Criticが追加Taskも撤回も提案しなかった場合の停止理由。
		// 以前はこの経路にも "repair rounds exhausted" と記録していたが、
		// 実際にはラウンドを消化せずに抜けており（1/2で終了）、
		// 停止理由の調査を誤誘導していたため文言を分離した。
		const bool mentionsNoRemediation = reason.find("no remediation proposed") != std::string::npos;
		assert(mentionsRepairExhausted || mentionsEarlyStopping || mentionsNoRemediation);

		// 打ち切り系の停止では、消化ラウンド数が併記されること。
		if (mentionsRepairExhausted || mentionsNoRemediation) {
			assert(result.stopInfo.contains("repairRoundsUsed"));
			assert(result.stopInfo.contains("maxRepairRounds"));
		}

		std::cout << "  - Scenario 2 (early stopping / repair exhausted, fallback report): OK" << std::endl;
	}

	// ============================================================
	// シナリオ3: 会話/雑談の高速パス
	// IntakeがrequestType="conversation"を返した場合、Planner以降の調査
	// パイプラインを一切呼ばず、DirectReplyの応答をそのまま報告として返すこと。
	// （実機失敗事例: 「あなたは何ができますか？」がPlanner 300秒タイムアウトを
	//   二度踏んだ問題への対処。Plannerが一度も呼ばれないことを直接検証する）
	// ============================================================
	{
		const std::string convDbPath = "/tmp/agentos_e2e_conv.db";
		RemoveDb(convDbPath);

		TaskStore store;
		Result openResult = store.Open(convDbPath);
		assert(openResult.ok);

		CapabilityRegistry registry;
		CommandPipeline pipeline(&registry);
		pipeline.RegisterTool(std::make_shared<DescribeEntityTool>());
		pipeline.RegisterTool(std::make_shared<GetWriteTraceTool>());
		pipeline.RegisterTool(std::make_shared<FindWritersTool>());

		MockLlmBackend llm;
		// Intake: 役割記述中の "Intake担当" が目印。requestType="conversation"を返す。
		llm.AddRule("Intake担当",
			"```json\n"
			"{\"goal\": \"AgentOSの能力について説明する\", \"symptoms\": [], \"constraints\": [], "
			"\"requiredCapabilities\": [], \"requestType\": \"conversation\"}\n"
			"```");
		// DirectReply: 役割記述中の "DirectReply担当" が目印。
		llm.AddRule("DirectReply担当",
			"```json\n"
			"{\"reply\": \"私はエンジンの調査ができます。Component値の観測やWrite Traceの取得などが可能です。\"}\n"
			"```");
		// Plannerルールはあえて登録しない → 呼ばれたら"{}"が返りIntakeでの
		// requestType欠落と同義になるため、Plannerが一切呼ばれないことを
		// GetCalls()で直接検証する。

		OrchestratorConfig config;
		config.maxRepairRounds = 2;
		Orchestrator orchestrator(&llm, &pipeline, &store, &registry, config);

		std::vector<std::string> stages;
		orchestrator.SetProgressCallback([&](const std::string& stage, const Json& detail) {
			(void)detail;
			stages.push_back(stage);
		});

		const OrchestratorResult result = orchestrator.RunSession("あなたは何ができますか？");

		assert(result.completed);
		assert(result.report.find("私はエンジンの調査ができます") != std::string::npos);

		// Plan/Retrieve等の調査ステージへ進んでいないこと
		bool sawPlanStage = false;
		for (const auto& st : stages) {
			if (st == "Plan" || st == "Retrieve" || st == "Reason" || st == "Critic" || st == "Synthesize") {
				sawPlanStage = true;
			}
		}
		assert(!sawPlanStage);

		// PlannerのLLM呼び出しが一度も発生していないこと（目印: "Planner担当"）
		const std::vector<std::pair<std::string, std::string>> calls = llm.GetCalls();
		bool sawPlannerCall = false;
		for (const auto& call : calls) {
			if (call.first.find("Planner担当") != std::string::npos) {
				sawPlannerCall = true;
			}
		}
		assert(!sawPlannerCall);

		// --- Store検証: DirectReply種別のTaskがSucceededで記録されていること ---
		const Json summary = store.GetSessionSummary(result.sessionId);
		assert(summary.at("tasksByState").contains("Succeeded"));

		bool foundDirectReplyTask = false;
		const std::vector<TaskRow> succeededTasks = store.GetTasksByState(result.sessionId, TaskState::Succeeded);
		for (const auto& task : succeededTasks) {
			if (task.type == "DirectReply") {
				foundDirectReplyTask = true;
			}
		}
		assert(foundDirectReplyTask);

		std::cout << "  - Scenario 3 (conversation fast path, no Planner call): OK" << std::endl;
	}

	// ============================================================
	// シナリオ4: クイックToolパス（3-tier fast path）
	// 実機失敗事例「このシーンのEntityを一覧して」を模す: Intakeが実データ要求を
	// 誤ってrequestType="conversation"と判定しても、DirectReplyが検証済みRead
	// Toolを1回だけ要求し、Orchestratorが決定的検証を経て実行→要約まで完了する
	// ことで、旧来の「実行していないのに実行したと言う」劣化を防げていること。
	// ============================================================
	{
		const std::string quickDbPath = "/tmp/agentos_e2e_quick.db";
		RemoveDb(quickDbPath);

		TaskStore store;
		Result openResult = store.Open(quickDbPath);
		assert(openResult.ok);

		CapabilityRegistry registry;
		CommandPipeline pipeline(&registry);
		pipeline.RegisterTool(std::make_shared<ListEntitiesTool>());

		MockLlmBackend llm;
		// Intake: 実データ要求だが意図的にconversationへ誤判定させる（EVIDENCE再現）。
		llm.AddRule("Intake担当",
			"```json\n"
			"{\"goal\": \"シーンのEntity一覧を確認する\", \"symptoms\": [], \"constraints\": [], "
			"\"requiredCapabilities\": [], \"requestType\": \"conversation\"}\n"
			"```");
		// DirectReply: 役割記述中の "DirectReply担当" が目印。toolCallでListEntitiesを要求する。
		llm.AddRule("DirectReply担当",
			"```json\n"
			"{\"reply\": null, \"toolCall\": {\"tool\": \"ListEntities\", \"arguments\": {}}, "
			"\"escalate\": false}\n"
			"```");
		// FormatToolResult: 役割記述中の "Reporter担当" が目印。
		llm.AddRule("Reporter担当",
			"```json\n"
			"{\"reply\": \"生存Entityは Boss と Player の2件です。\"}\n"
			"```");
		// Plannerルールはあえて登録しない → クイックパスがPlannerへ到達しないことを検証する。

		OrchestratorConfig config;
		config.maxRepairRounds = 2;
		Orchestrator orchestrator(&llm, &pipeline, &store, &registry, config);

		const OrchestratorResult result = orchestrator.RunSession("このシーンのEntityを一覧して");

		assert(result.completed);
		assert(result.report.find("Boss") != std::string::npos);
		assert(result.report.find("Player") != std::string::npos);
		assert(result.stopInfo.value("reason", std::string()) == "quick tool path");

		// --- 監査ログ: ListEntitiesがissuer "QuickPath" によりOkで正確に1回だけ実行されたこと ---
		const auto auditLog = pipeline.GetAuditLog();
		int quickPathOkCount = 0;
		for (const auto& entry : auditLog) {
			if (entry.first.tool == "ListEntities" && entry.first.issuer == "QuickPath" &&
			    entry.second.status == CommandStatus::Ok) {
				++quickPathOkCount;
			}
		}
		assert(quickPathOkCount == 1);
		assert(auditLog.size() == 1); // クイックパスで許すTool実行は最大1回

		// --- Plannerが一度も呼ばれていないこと（目印: "Planner担当"） ---
		const std::vector<std::pair<std::string, std::string>> calls = llm.GetCalls();
		bool sawPlannerCall = false;
		for (const auto& call : calls) {
			if (call.first.find("Planner担当") != std::string::npos) {
				sawPlannerCall = true;
			}
		}
		assert(!sawPlannerCall);

		std::cout << "  - Scenario 4 (quick tool path rescues misclassified request): OK" << std::endl;
	}

	// ============================================================
	// シナリオ5: エスカレーション
	// DirectReplyがescalate=trueを返した場合、DirectReplyは何もTaskを作らず・
	// 何もSubmitせずreturnもしない。Intake結果（変数intake）を再利用したまま
	// 通常の調査パイプライン（Planner以降）へフォールスルーし、最後まで完了する
	// こと。MockLlm呼び出し履歴に "Planner担当" が実際に現れることを直接検証する。
	// ============================================================
	{
		const std::string escDbPath = "/tmp/agentos_e2e_esc.db";
		RemoveDb(escDbPath);

		TaskStore store;
		Result openResult = store.Open(escDbPath);
		assert(openResult.ok);

		CapabilityRegistry registry;
		CommandPipeline pipeline(&registry);
		pipeline.RegisterTool(std::make_shared<DescribeEntityTool>());
		pipeline.RegisterTool(std::make_shared<GetWriteTraceTool>());
		pipeline.RegisterTool(std::make_shared<FindWritersTool>());

		MockLlmBackend llm;
		// Intake: 意図的にconversationへ誤判定させる。
		llm.AddRule("Intake担当",
			"```json\n"
			"{\"goal\": \"Bossが Idle 状態なのに Velocity.y が異常な値になっている原因Systemを特定する\", "
			"\"symptoms\": [\"Boss.Velocity.y が Idle 中に 8.2 になっている\"], \"constraints\": [], "
			"\"requiredCapabilities\": [], \"requestType\": \"conversation\"}\n"
			"```");
		// DirectReply: escalate=trueを返す（reply/toolCallともにnull）。
		llm.AddRule("DirectReply担当",
			"```json\n"
			"{\"reply\": null, \"toolCall\": null, \"escalate\": true}\n"
			"```");
		// 以降はシナリオ1（ハッピーパス）と同一内容の調査パイプライン用モック。
		// Intakeルールはconversation誤判定版を使うため重複登録しない。
		llm.AddRule("Planner担当",
			"```json\n"
			"{\"tasks\": ["
			"{\"taskId\": \"T1\", \"type\": \"RuntimeObservation\", "
			"\"description\": \"Boss entityの現況とVelocity書込元候補を調べる\", "
			"\"dependencies\": [], \"allowedTools\": [\"DescribeEntity\", \"FindWriters\"], "
			"\"searchHints\": [\"Boss\", \"Velocity\"]}, "
			"{\"taskId\": \"T2\", \"type\": \"Trace\", "
			"\"description\": \"Velocityの書込トレースを取得する\", "
			"\"dependencies\": [\"T1\"], \"allowedTools\": [\"GetWriteTrace\"], "
			"\"searchHints\": [\"Velocity\"]}"
			"]}\n"
			"```");
		llm.AddRule("\"taskId\": \"T1\"",
			"```json\n"
			"{\"commands\": ["
			"{\"tool\": \"DescribeEntity\", \"arguments\": {\"entityName\": \"Boss\"}}, "
			"{\"tool\": \"FindWriters\", \"arguments\": {\"component\": \"Velocity\"}}"
			"]}\n"
			"```");
		llm.AddRule("\"taskId\": \"T2\"",
			"```json\n"
			"{\"commands\": ["
			"{\"tool\": \"GetWriteTrace\", \"arguments\": {\"entityName\": \"Boss\", \"component\": \"Velocity\"}}"
			"]}\n"
			"```");
		llm.AddRule("Reasoning担当",
			"```json\n"
			"{\"hypotheses\": ["
			"{\"description\": \"JumpAttack終了時にVelocity.yがリセットされていない\", \"rubricBase\": 0.9, "
			"\"supports\": [1, 2, 3], \"contradicts\": [], \"missingEvidence\": []}"
			"]}\n"
			"```");
		llm.AddRule("Critic担当",
			"```json\n"
			"{\"scores\": {\"evidenceCoverage\": 0.9, \"contradictionHandling\": 1.0, "
			"\"causalCompleteness\": 0.8, \"testability\": 0.7}, "
			"\"failures\": [], \"additionalTasksSuggested\": []}\n"
			"```");
		llm.AddRule("Synthesis担当",
			"```json\n"
			"{\"report\": \"## 調査結果\\n\\nBossJumpAttackSystem がJumpAttack中にVelocity.y=8.2を書き込んだ後、"
			"Idle状態へ遷移してもリセットする処理が存在しないため、Idle中もVelocity.yが残存していると考えられます。\"}\n"
			"```");

		OrchestratorConfig config;
		config.maxRepairRounds = 2;
		Orchestrator orchestrator(&llm, &pipeline, &store, &registry, config);

		const OrchestratorResult result = orchestrator.RunSession(
			"Bossが Idle 状態なのに Velocity.y が 8.2 のままで、想定外に落下しているように見える。"
			"原因Systemを特定してほしい。");

		assert(result.completed);

		const std::vector<std::pair<std::string, std::string>> calls = llm.GetCalls();
		bool sawPlannerCall = false;
		for (const auto& call : calls) {
			if (call.first.find("Planner担当") != std::string::npos) {
				sawPlannerCall = true;
			}
		}
		assert(sawPlannerCall);

		std::cout << "  - Scenario 5 (escalation falls through to full investigation): OK" << std::endl;
	}

	std::cout << "AgentOSVerticalSliceSmokeTest: OK" << std::endl;
	return 0;
}
