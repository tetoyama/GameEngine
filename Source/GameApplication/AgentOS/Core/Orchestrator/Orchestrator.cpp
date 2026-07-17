// =======================================================================
//
// Orchestrator.cpp
//
// =======================================================================
#include "Orchestrator.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "EarlyStopping.h"
#include "Supervisor.h"
#include "../Agents/AgentContext.h"
#include "../Agents/CriticAgent.h"
#include "../Agents/IntakeAgent.h"
#include "../Agents/PlannerAgent.h"
#include "../Agents/ReasoningAgent.h"
#include "../Agents/RetrievalWorker.h"
#include "../Agents/SynthesisAgent.h"
#include "../Evidence/Evidence.h"
#include "../Evidence/EvidenceBuilder.h"
#include "../Llm/PromptTemplates.h"
#include "../Logic/LogicGraph.h"

namespace agentos {

namespace {

// ---------------------------------
// TaskStoreAuditSink
// CommandPipeline::Submit()の結果を全てCommandテーブルへ永続化する。
// どのAgent/Workerが発行したCommandかに関わらず一律に記録することで、
// 監査ログの取りこぼしを防ぐ（構想§8）。
// ---------------------------------
class TaskStoreAuditSink : public IAuditSink {
public:
	explicit TaskStoreAuditSink(TaskStore* store) : store_(store) {}

	void OnCommand(const CommandRequest& request, const CommandResult& result) override {
		const std::string status = ToString(result.status);
		Json resultJson = result.IsOk() ? result.payload : Json::object({{"error", result.error}});
		store_->RecordCommand(request.taskId, request.issuer, request.tool, request.arguments,
		                       status, status, resultJson);
	}

private:
	TaskStore* store_ = nullptr;
};

// ---------------------------------
// Plan中のtasks配列を依存関係に従ってトポロジカル順に並べる（Kahn法）。
// PlannerAgentが循環無し・依存先実在をすでに検証済みの前提で使う。
// 万一の不整合があっても無限ループはせず、残りを末尾へ追加して終了する。
// ---------------------------------
std::vector<Json> TopologicalOrder(const Json& tasks) {
	std::vector<Json> result;
	if (!tasks.is_array()) {
		return result;
	}

	std::vector<std::string> order;
	std::unordered_map<std::string, Json> byId;
	std::unordered_map<std::string, std::vector<std::string>> dependents;
	std::unordered_map<std::string, int> indegree;

	for (const auto& t : tasks) {
		const std::string id = t.value("taskId", std::string());
		order.push_back(id);
		byId[id] = t;
		indegree[id] = 0;
	}
	for (const auto& t : tasks) {
		const std::string id = t.value("taskId", std::string());
		if (t.contains("dependencies") && t.at("dependencies").is_array()) {
			for (const auto& d : t.at("dependencies")) {
				if (!d.is_string()) {
					continue;
				}
				const std::string depId = d.get<std::string>();
				if (byId.count(depId) == 0) {
					continue;
				}
				dependents[depId].push_back(id);
				indegree[id] += 1;
			}
		}
	}

	std::vector<std::string> queue;
	for (const auto& id : order) {
		if (indegree[id] == 0) {
			queue.push_back(id);
		}
	}

	std::unordered_set<std::string> visited;
	std::size_t qi = 0;
	while (qi < queue.size()) {
		const std::string id = queue[qi++];
		if (visited.count(id) != 0) {
			continue;
		}
		visited.insert(id);
		result.push_back(byId[id]);
		for (const auto& dependent : dependents[id]) {
			indegree[dependent] -= 1;
			if (indegree[dependent] == 0) {
				queue.push_back(dependent);
			}
		}
	}

	// 防御的フォールバック: 循環等で取りこぼしたタスクは末尾に追加する。
	for (const auto& id : order) {
		if (visited.count(id) == 0) {
			result.push_back(byId[id]);
		}
	}

	return result;
}

// ---------------------------------
// ScopedBudgetTracker
// pipeline->SetBudgetTracker()はraw pointerを保持するだけなので、RunSession
// 内のローカルBudgetTrackerを指したまま関数を抜けるとダングリングポインタに
// なる。RAIIで関数を抜ける際に必ずnullptrへ戻す（早期returnが複数あるため）。
// ---------------------------------
class ScopedBudgetTracker {
public:
	ScopedBudgetTracker(CommandPipeline* pipeline, BudgetTracker* tracker) : pipeline_(pipeline) {
		pipeline_->SetBudgetTracker(tracker);
	}
	~ScopedBudgetTracker() {
		pipeline_->SetBudgetTracker(nullptr);
	}
	ScopedBudgetTracker(const ScopedBudgetTracker&) = delete;
	ScopedBudgetTracker& operator=(const ScopedBudgetTracker&) = delete;

private:
	CommandPipeline* pipeline_ = nullptr;
};

} // namespace

Orchestrator::Orchestrator(ILlmBackend* llm, CommandPipeline* pipeline, TaskStore* store,
                            CapabilityRegistry* capabilityRegistry, OrchestratorConfig config)
	: llm_(llm), pipeline_(pipeline), store_(store), capabilityRegistry_(capabilityRegistry), config_(config) {}

void Orchestrator::SetProgressCallback(std::function<void(const std::string&, const Json&)> callback) {
	progressCallback_ = std::move(callback);
}

void Orchestrator::ReportProgress(const std::string& stage, const Json& detail) {
	if (progressCallback_) {
		progressCallback_(stage, detail);
	}
}

CapabilityToken Orchestrator::GetLastIssuedToken() const {
	return lastToken_;
}

OrchestratorResult Orchestrator::RunSession(const std::string& userRequest) {
	OrchestratorResult sessionResult;

	BudgetTracker budgetTracker(config_.budget);
	ScopedBudgetTracker budgetGuard(pipeline_, &budgetTracker); // 関数を抜けたら必ずnullptrに戻す

	// --- Session作成 ---
	const Json goalJson = Json::object({{"userRequest", userRequest}});
	const SessionId sessionId = store_->CreateSession(goalJson);
	sessionResult.sessionId = sessionId;
	if (sessionId == kInvalidId) {
		sessionResult.completed = false;
		sessionResult.stopInfo = Json::object({{"reason", "CreateSession failed"}});
		sessionResult.report = "セッションを作成できませんでした。";
		return sessionResult;
	}
	store_->UpdateSessionState(sessionId, "Running");

	// --- Agentトークン発行（Observe止まり。Modifyは絶対に付与しない） ---
	const CapabilityToken token = capabilityRegistry_->IssueToken("Orchestrator", {"*"}, PermissionLevel::Observe);
	lastToken_ = token;

	// --- 監査シンク登録: pipeline経由の全CommandをTaskStoreへ永続化する ---
	auto auditSink = std::make_shared<TaskStoreAuditSink>(store_);
	pipeline_->AddAuditSink(auditSink);

	AgentContext ctx;
	ctx.llm = llm_;
	ctx.pipeline = pipeline_;
	ctx.store = store_;
	ctx.budget = &budgetTracker;
	ctx.token = token;
	ctx.sessionId = sessionId;

	Supervisor supervisor(store_);

	// --- Intake（root task として記録） ---
	const TaskId rootTaskId = store_->CreateTask(sessionId, kInvalidId, "Intake", Json::object(), 0);
	ReportProgress("Intake", Json::object({{"taskId", rootTaskId}}));
	supervisor.StartTask(rootTaskId);

	Json intake;
	Result intakeResult = IntakeAgent::Run(ctx, userRequest, &intake);
	if (!intakeResult) {
		supervisor.FailTask(rootTaskId, intakeResult.error);
		store_->UpdateSessionState(sessionId, "Stopped");
		sessionResult.completed = false;
		sessionResult.stopInfo = Json::object({{"reason", "intake failed: " + intakeResult.error}});
		sessionResult.report = "調査を開始できませんでした（Intake失敗）: " + intakeResult.error;
		return sessionResult;
	}
	supervisor.CompleteTask(rootTaskId, intake);

	// --- 会話/雑談/能力質問など: 調査パイプライン（Plan〜Synthesis）を丸ごと
	//     スキップする3-tier fast path。実機ログで「あなたは何ができますか？」のような
	//     conversational要求がPlanner 300秒タイムアウトを二度も踏んだ事例への対処。
	//     さらに「このシーンのEntityを一覧して」がIntakeに誤ってconversation判定
	//     された事例（実データ要求なのにToolを一切実行できず終わる）への対処として、
	//     DirectReplyへ決定的検証付きのRead専用Tool呼び出しを1回だけ許可する。
	//     クイックパスで許すTool実行は最大1回（DirectReply→[Submit]→FormatToolResult
	//     で、Intake後のLLM呼び出しは最大2回）というハードキャップを設ける。
	//     複数Toolや多段調査が必要と判定された場合（escalate）は、ここでは何も
	//     実行・returnせず、そのまま下の通常調査パイプラインへフォールスルーする
	//     （Intake結果は再利用する＝Intakeをやり直さない）。
	const std::string requestType = intake.value("requestType", std::string("investigation"));
	if (requestType == "conversation") {
		ReportProgress("reply", Json::object());

		const Json toolCatalogForReply = pipeline_->DescribeTools();
		const std::string compactCatalog = prompts::CompactToolCatalog(toolCatalogForReply);
		const PromptPair replyPrompt = prompts::DirectReply(userRequest, compactCatalog);

		Json replyJson;
		Result replyResult = CallLlmJson(ctx, replyPrompt, &replyJson);

		// 新スキーマ: {reply: string|null, toolCall: {tool, arguments}|null, escalate: boolean}
		// 旧スキーマ（{reply: string}のみ）で書かれた既存モックとの後方互換のため、
		// toolCall/escalateの欠損・null・型不一致はいずれも「無し/false」として
		// 寛容に扱う（LLM出力・旧テストモックはuntrusted）。
		std::string reply;
		bool hasReply = false;
		Json toolCallJson;
		bool hasToolCall = false;
		bool escalate = false;
		std::string llmError;

		if (!replyResult) {
			llmError = replyResult.error;
		} else if (!replyJson.is_object()) {
			llmError = "DirectReply応答がJSONオブジェクトではありません";
		} else {
			if (replyJson.contains("reply") && replyJson.at("reply").is_string() &&
			    !replyJson.at("reply").get<std::string>().empty()) {
				reply = replyJson.at("reply").get<std::string>();
				hasReply = true;
			}
			if (replyJson.contains("toolCall") && replyJson.at("toolCall").is_object()) {
				toolCallJson = replyJson.at("toolCall");
				hasToolCall = true;
			}
			if (replyJson.contains("escalate") && replyJson.at("escalate").is_boolean()) {
				escalate = replyJson.at("escalate").get<bool>();
			}
		}

		if (!replyResult) {
			// LLM呼び出し自体（JSON抽出含む）が失敗 → 決定的フォールバックで即終了。
			// エスカレーションはしない（Intake自体は成功しているが、DirectReplyが
			// 応答不能な状況で調査パイプラインへ進んでも同じLLMが使われる可能性が高い）。
			const std::string fallback = "会話応答の生成に失敗しました: " + llmError;
			const TaskId replyTaskId = store_->CreateTask(sessionId, rootTaskId, "DirectReply", Json::object(), 1);
			supervisor.StartTask(replyTaskId);
			supervisor.CompleteTask(replyTaskId, Json::object({{"reply", fallback}}));
			store_->UpdateSessionState(sessionId, "Stopped");

			sessionResult.completed = false;
			sessionResult.report = fallback;
			sessionResult.stopInfo =
				Json::object({{"reason", "conversational fast path: reply generation failed: " + llmError}});
			return sessionResult;
		}

		// --- toolCallの決定的検証（LLMを介さない）: catalogに実在しRead権限であること ---
		std::string toolName;
		Json toolArgs = Json::object();
		bool toolValid = false;
		if (hasToolCall) {
			toolName = toolCallJson.value("tool", std::string());
			if (toolCallJson.contains("arguments") && toolCallJson.at("arguments").is_object()) {
				toolArgs = toolCallJson.at("arguments");
			}
			for (const auto& t : toolCatalogForReply) {
				if (t.is_object() && t.value("name", std::string()) == toolName &&
				    t.value("requiredPermission", std::string()) == "Read") {
					toolValid = true;
					break;
				}
			}
			if (!toolValid) {
				// catalog外のTool、またはRead以外の権限を要求 → escalate扱いに落とす。
				escalate = true;
			}
		}

		if (escalate) {
			// --- エスカレーション: 通常の調査パイプラインへフォールスルー ---
			ReportProgress("escalate", Json::object());
			// ここではreturnしない。下の通常調査パイプライン（Planner以降）が
			// このIntake結果（変数intake）を再利用してそのまま実行される。
		} else if (hasToolCall && toolValid) {
			// --- クイックToolパス: DirectReplyの提案Toolを検証済みで1回だけ実行する ---
			const TaskId replyTaskId = store_->CreateTask(sessionId, rootTaskId, "DirectReply", Json::object(), 1);
			supervisor.StartTask(replyTaskId);
			supervisor.CompleteTask(replyTaskId, replyJson);

			const Json toolTaskSpec = Json::object({{"tool", toolName}, {"arguments", toolArgs}});
			const TaskId toolTaskId = store_->CreateTask(sessionId, rootTaskId, "QuickTool", toolTaskSpec, 1);
			supervisor.StartTask(toolTaskId);

			CommandRequest request;
			request.taskId = toolTaskId;
			request.issuer = "QuickPath";
			request.tool = toolName;
			request.arguments = toolArgs;
			request.capability = ctx.token;
			const CommandResult cmdResult = pipeline_->Submit(request);

			if (!cmdResult.IsOk()) {
				supervisor.FailTask(toolTaskId, cmdResult.error);
				const std::string fallback = "Tool実行に失敗しました: " + cmdResult.error;
				store_->UpdateSessionState(sessionId, "Stopped");

				sessionResult.completed = false;
				sessionResult.report = fallback;
				sessionResult.stopInfo =
					Json::object({{"reason", "quick tool path: execution failed: " + cmdResult.error}});
				return sessionResult;
			}
			supervisor.CompleteTask(toolTaskId, cmdResult.payload);

			// --- Tool結果の要約（Reporter担当）: クイックパス内2回目かつ最後のLLM呼び出し ---
			const PromptPair formatPrompt = prompts::FormatToolResult(userRequest, toolName, cmdResult.payload);
			Json formatJson;
			Result formatResult = CallLlmJson(ctx, formatPrompt, &formatJson);

			std::string finalReply;
			if (formatResult && formatJson.is_object() && formatJson.contains("reply") &&
			    formatJson.at("reply").is_string() && !formatJson.at("reply").get<std::string>().empty()) {
				// スキーマ違反でtoolCall等が混ざっていても、reply文字列があればそれを使う。
				finalReply = formatJson.at("reply").get<std::string>();
			} else {
				// 生成失敗・スキーマ違反いずれも決定的フォールバックへ落とす。
				finalReply = "Tool " + toolName + " の実行結果: " + prompts::Truncate(cmdResult.payload.dump(2));
			}

			const TaskId reportTaskId = store_->CreateTask(sessionId, rootTaskId, "FormatToolResult", Json::object(), 1);
			supervisor.StartTask(reportTaskId);
			supervisor.CompleteTask(reportTaskId, Json::object({{"reply", finalReply}}));

			store_->UpdateSessionState(sessionId, "Completed");

			sessionResult.completed = true;
			sessionResult.report = finalReply;
			sessionResult.stopInfo = Json::object({{"reason", "quick tool path"}});
			return sessionResult;
		} else if (hasReply) {
			// --- 既存の挙動: toolCall無しのプレーンなreply ---
			const TaskId replyTaskId = store_->CreateTask(sessionId, rootTaskId, "DirectReply", Json::object(), 1);
			supervisor.StartTask(replyTaskId);
			supervisor.CompleteTask(replyTaskId, Json::object({{"reply", reply}}));

			store_->UpdateSessionState(sessionId, "Completed");

			sessionResult.completed = true;
			sessionResult.report = reply;
			sessionResult.stopInfo = Json::object({{"reason", "conversational fast path"}});
			return sessionResult;
		} else {
			// reply/toolCall/escalateのいずれも有効な形で得られなかった → 決定的フォールバック。
			const std::string fallback =
				"会話応答の生成に失敗しました: DirectReply応答にreply/toolCall/escalateのいずれも"
				"有効な値が含まれていません";
			const TaskId replyTaskId = store_->CreateTask(sessionId, rootTaskId, "DirectReply", Json::object(), 1);
			supervisor.StartTask(replyTaskId);
			supervisor.CompleteTask(replyTaskId, Json::object({{"reply", fallback}}));
			store_->UpdateSessionState(sessionId, "Stopped");

			sessionResult.completed = false;
			sessionResult.report = fallback;
			sessionResult.stopInfo = Json::object(
				{{"reason", "conversational fast path: reply generation failed: no reply/toolCall/escalate"}});
			return sessionResult;
		}
	}

	// --- Planner ---
	ReportProgress("Plan", Json::object());
	const Json toolCatalog = pipeline_->DescribeTools();
	Json plan;
	Result planResult = PlannerAgent::Run(ctx, intake, toolCatalog, &plan);
	if (!planResult) {
		store_->UpdateSessionState(sessionId, "Stopped");
		sessionResult.completed = false;
		sessionResult.stopInfo = Json::object({{"reason", "planning failed: " + planResult.error}});
		sessionResult.report = "計画の作成に失敗しました: " + planResult.error;
		return sessionResult;
	}

	// --- Plan Taskのトポロジカル実行 ---
	EvidenceBuilder evidenceBuilder;
	const std::vector<Json> orderedTasks = TopologicalOrder(plan.value("tasks", Json::array()));

	for (const Json& taskSpec : orderedTasks) {
		const std::string planTaskId = taskSpec.value("taskId", std::string());
		const std::string type = taskSpec.value("type", std::string());

		const TaskId storeTaskId = store_->CreateTask(sessionId, rootTaskId, type, taskSpec, 1);
		ReportProgress("Retrieve", Json::object({{"taskId", storeTaskId}, {"planTaskId", planTaskId}, {"type", type}}));

		Result startResult = supervisor.StartTask(storeTaskId);
		if (!startResult) {
			continue; // 状態遷移自体が拒否された（通常到達しない防御的分岐）
		}

		Result depthCheck = supervisor.CheckDepth(1, config_.budget);
		if (!depthCheck) {
			supervisor.FailTask(storeTaskId, depthCheck.error);
			continue;
		}

		if (type == "Analysis") {
			// AnalysisタスクはWorkerを起動しない（ReasoningAgentが扱う）。
			supervisor.CompleteTask(storeTaskId, Json::object({{"note", "handled by ReasoningAgent"}}));
			continue;
		}

		evidenceBuilder.MarkPlannedTask(storeTaskId);

		std::vector<Evidence> evidences;
		Json summary;
		Result workerResult = RetrievalWorker::Run(ctx, storeTaskId, taskSpec, &evidences, &summary);
		for (auto& e : evidences) {
			evidenceBuilder.Add(e);
		}

		if (!workerResult) {
			supervisor.FailTask(storeTaskId, workerResult.error);
		} else {
			supervisor.CompleteTask(storeTaskId, summary);
		}
	}

	// --- Evidence統合 ---
	EvidenceBuilder::BuiltEvidence built = evidenceBuilder.Build();
	Json builtJson = EvidenceBuilder::ToJson(built);

	// --- Reasoning ---
	ReportProgress("Reason", Json::object());
	LogicGraph graph;
	Json reasonRaw;
	ReasoningAgent::Run(ctx, builtJson, &graph, &reasonRaw); // 失敗しても空グラフのまま続行する

	Json rankedJson = graph.ToJson();

	// --- Critic ---
	ReportProgress("Critic", Json::object());
	CriticVerdict verdict;
	CriticAgent::Run(ctx, rankedJson, builtJson, &verdict);

	// --- Repair（不足時のみ。EarlyStoppingで打ち切る） ---
	EarlyStopping earlyStopping;
	int previousEvidenceCount = static_cast<int>(built.evidences.size());
	std::vector<std::string> previousFailures = verdict.failures;

	Json stopInfo = Json::object();
	bool stopped = false;
	int repairRoundsUsed = 0;

	while (!verdict.pass && repairRoundsUsed < config_.maxRepairRounds) {
		ReportProgress("Repair", Json::object({{"round", repairRoundsUsed + 1}}));

		const Json additional = verdict.additionalTasks;
		if (!additional.is_array() || additional.empty()) {
			stopInfo = Json::object({{"reason", "repair rounds exhausted (critic suggested no additional tasks)"}});
			stopped = true;
			break;
		}

		int added = 0;
		const Json fullCatalogForRepair = pipeline_->DescribeTools();
		Json allToolNames = Json::array();
		if (fullCatalogForRepair.is_array()) {
			for (const auto& t : fullCatalogForRepair) {
				if (t.contains("name")) {
					allToolNames.push_back(t.at("name"));
				}
			}
		}

		for (const auto& add : additional) {
			if (added >= 2) {
				break;
			}
			if (!add.is_object()) {
				continue;
			}
			const std::string addType = add.value("type", std::string());
			if (addType != "RuntimeObservation" && addType != "CodeSearch" && addType != "Trace") {
				continue; // Analysis/不明種別は追加検索Taskにできない
			}

			Json addSpec = Json::object();
			addSpec["taskId"] = "repair_" + std::to_string(repairRoundsUsed) + "_" + std::to_string(added);
			addSpec["type"] = addType;
			addSpec["description"] = add.value("description", std::string());
			addSpec["allowedTools"] = allToolNames; // 修復タスクはToken自体がObserve止まりなので全許可でも安全

			const TaskId storeTaskId = store_->CreateTask(sessionId, rootTaskId, addType, addSpec, 1);
			supervisor.StartTask(storeTaskId);
			evidenceBuilder.MarkPlannedTask(storeTaskId);

			std::vector<Evidence> evidences;
			Json summary;
			Result wr = RetrievalWorker::Run(ctx, storeTaskId, addSpec, &evidences, &summary);
			for (auto& e : evidences) {
				evidenceBuilder.Add(e);
			}
			if (!wr) {
				supervisor.FailTask(storeTaskId, wr.error);
			} else {
				supervisor.CompleteTask(storeTaskId, summary);
			}
			++added;
		}

		built = evidenceBuilder.Build();
		builtJson = EvidenceBuilder::ToJson(built);

		ReportProgress("Reason", Json::object({{"round", repairRoundsUsed + 1}}));
		LogicGraph newGraph;
		Json reasonRaw2;
		ReasoningAgent::Run(ctx, builtJson, &newGraph, &reasonRaw2);
		graph = std::move(newGraph);
		rankedJson = graph.ToJson();

		ReportProgress("Critic", Json::object({{"round", repairRoundsUsed + 1}}));
		CriticAgent::Run(ctx, rankedJson, builtJson, &verdict);

		LogicNodeId newTop = kInvalidId;
		if (rankedJson.contains("hypotheses") && rankedJson.at("hypotheses").is_array() &&
		    !rankedJson.at("hypotheses").empty()) {
			newTop = rankedJson.at("hypotheses")[0].value("id", kInvalidId);
		}
		const int newEvidenceCount = static_cast<int>(built.evidences.size()) - previousEvidenceCount;
		const bool sameFailureRepeated = !verdict.failures.empty() && verdict.failures == previousFailures;

		earlyStopping.RecordRound(newEvidenceCount, newTop, static_cast<int>(built.contradictions.size()),
		                           sameFailureRepeated);

		previousEvidenceCount = static_cast<int>(built.evidences.size());
		previousFailures = verdict.failures;
		++repairRoundsUsed;

		const EarlyStopping::StopDecision decision = earlyStopping.Evaluate(budgetTracker);
		if (decision.stop) {
			stopInfo = Json::object({{"reason", "early stopping: " + decision.reason}});
			stopped = true;
			break;
		}
	}

	if (!stopped) {
		stopInfo = verdict.pass
			? Json::object({{"reason", "critic passed"}})
			: Json::object({{"reason", "repair rounds exhausted"}});
	}

	// --- Synthesis ---
	ReportProgress("Synthesize", Json::object());
	std::string report;
	SynthesisAgent::Run(ctx, builtJson, rankedJson, stopInfo, &report);

	store_->UpdateSessionState(sessionId, verdict.pass ? "Completed" : "Stopped");

	sessionResult.completed = verdict.pass;
	sessionResult.report = report;
	sessionResult.stopInfo = stopInfo;
	sessionResult.rankedHypotheses = rankedJson;
	return sessionResult;
}

} // namespace agentos
