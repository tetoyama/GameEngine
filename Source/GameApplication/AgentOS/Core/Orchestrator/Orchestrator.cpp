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
#include "../Command/CommandSchema.h"
#include "../Evidence/Evidence.h"
#include "../Evidence/EvidenceBuilder.h"
#include "../Llm/PromptTemplates.h"
#include "../Logic/LogicGraph.h"

namespace agentos {

namespace {

// ---------------------------------
// 修復Taskの引数を「計画の時点で」検証する
// ---------------------------------
//
// Criticが提案する追加Taskの引数は、そのままではTool스キーマに合わないことがある。
// 実機で観測した例:
//   - Evidenceのペイロードに押される内部項目 requestRevision を引数へ混入させる
//   - fileという正式名の代わりにtargetFileを創作する
//
// これらは「聞き方の誤り」であって「調べても無かった」ではない。
// 実行して失敗Evidenceとして記録すると、カバレッジが下がり失敗数が増え、
// 制御ループが自分の出したノイズを観測結果として食べることになる。
//
// そこで実行前に潰す。ICommandExecutorがPreconditionとExecuteを分けているのは
// 元々このDry Runのためなので、その設計意図に沿う形にする。
//   - スキーマに無い項目は取り除く（残りが有効ならTaskは活かす）
//   - 必須項目が欠ける／Tool名が不明なら、Task自体を作らない
//
// 戻り値: このTaskを実行してよいか。
bool SanitizeRepairCommand(const Json& toolCatalog, Json* command, std::string* rejectReason) {
	if (command == nullptr || !command->is_object()) {
		if (rejectReason) *rejectReason = "command is not an object";
		return false;
	}
	const std::string toolName = command->value("tool", std::string());
	if (toolName.empty()) {
		if (rejectReason) *rejectReason = "tool name is empty";
		return false;
	}

	// Tool記述子を引く
	Json schema;
	bool found = false;
	if (toolCatalog.is_array()) {
		for (const Json& entry : toolCatalog) {
			if (!entry.is_object()) continue;
			if (entry.value("name", std::string()) != toolName) continue;
			schema = entry.value("argumentSchema", Json::object());
			found = true;
			break;
		}
	}
	if (!found) {
		if (rejectReason) *rejectReason = "unknown tool: " + toolName;
		return false;
	}

	Json arguments = command->value("arguments", Json::object());
	if (!arguments.is_object()) arguments = Json::object();

	// スキーマに無い項目を落とす。
	// queryが正しいのにrequestRevisionが付いていただけで捨てるのは惜しい。
	if (schema.is_object()) {
		Json cleaned = Json::object();
		for (auto it = arguments.begin(); it != arguments.end(); ++it) {
			if (schema.contains(it.key())) cleaned[it.key()] = it.value();
		}
		arguments = std::move(cleaned);
	}

	// 残った引数がスキーマを満たすか（必須欠けなど）を実行前に確認する。
	const Result validation = SchemaValidator::Validate(arguments, schema);
	if (!validation) {
		if (rejectReason) *rejectReason = validation.error;
		return false;
	}

	(*command)["arguments"] = std::move(arguments);
	return true;
}

// descriptionへ埋め込まれたREPAIR_COMMANDを取り出して検証し、書き戻す。
// 埋め込みが無いTask（Workerがdescriptionから自力でCommandを組む）はそのまま通す。
bool SanitizeRepairDescription(const Json& toolCatalog, std::string* description, std::string* rejectReason) {
	if (description == nullptr) return true;

	static const std::string marker = "REPAIR_COMMAND ";
	const std::size_t pos = description->find(marker);
	if (pos == std::string::npos) return true; // 埋め込み無し

	const std::string encoded = description->substr(pos + marker.size());
	Json command = Json::parse(encoded, nullptr, false);
	if (command.is_discarded() || !command.is_object()) {
		if (rejectReason) *rejectReason = "REPAIR_COMMAND is not valid JSON";
		return false;
	}

	if (!SanitizeRepairCommand(toolCatalog, &command, rejectReason)) return false;

	*description = description->substr(0, pos) + marker + command.dump();
	return true;
}

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

	// GetConversationHistoryツールは「今より前」の境界として現在セッションIDを要る。
	// ToolはAgentContextを受け取れないため、Worker thread単位で共有する。
	SetCurrentSessionId(sessionId);

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

	// --- 会話も調査も同じパイプラインを通る ---
	//
	// 以前はここに3段のfast path（DirectReply / quick tool path /
	// conversational fast path）があり、IntakeのrequestTypeとキーワード一致で
	// 調査パイプラインを丸ごと飛ばしていた。
	// 判定が外れるたびに語を足す構造になっており、実際に
	//   「私は誰ですか？」   → 身元をSceneに探しに行き修復2ラウンド空振り
	//   「私の名前はTaroです」→ 自己紹介が ResolveEntity(Taro) になった
	// という故障を出した（transcript_20260727_0208xx）。
	//
	// 会話応答自体をツール(Respond)にしたので、Plannerがツール一覧から
	// 普通に選べばよい。要求の種類を先回りで分類しない。
	// 完了かどうかは、その出力を見てCriticが判定する。

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

		// Task種別(type)は廃止した。Taskの実体は「どのToolを使うか」であり、
		// Toolを使わないTaskは実行しない。以前の "Analysis" 種別と同じ意味を
		// allowedTools が空かどうかで表す（PlannerAgent.cppの注記参照）。
		const Json allowedTools = taskSpec.value("allowedTools", Json::array());
		const bool usesTool = allowedTools.is_array() && !allowedTools.empty();
		const std::string taskLabel = usesTool ? "Retrieval" : "Analysis";

		const TaskId storeTaskId = store_->CreateTask(sessionId, rootTaskId, taskLabel, taskSpec, 1);
		ReportProgress("Retrieve", Json::object({{"taskId", storeTaskId}, {"planTaskId", planTaskId}}));

		Result startResult = supervisor.StartTask(storeTaskId);
		if (!startResult) {
			continue; // 状態遷移自体が拒否された（通常到達しない防御的分岐）
		}

		Result depthCheck = supervisor.CheckDepth(1, config_.budget);
		if (!depthCheck) {
			supervisor.FailTask(storeTaskId, depthCheck.error);
			continue;
		}

		if (!usesTool) {
			// Toolを使わないTaskはWorkerを起動しない（ReasoningAgentが扱う）。
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
	prompts::SetCurrentBuiltEvidence(builtJson);

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
		const Json obsolete = verdict.obsoleteTasks;
		const bool hasAdditions = additional.is_array() && !additional.empty();
		const bool hasRetirements = obsolete.is_array() && !obsolete.empty();

		// 修復には「追加」と「撤回」の2種類がある。
		// 撤回が無かった頃は、不要Taskの失敗1件がcoverage/tasksWithoutEvidence/
		// failedEvidenceCountを同時に踏み、追加をいくら重ねてもpassへ戻れなかった。
		if (!hasAdditions && !hasRetirements) {
			stopInfo = Json::object({
				{"reason", "no remediation proposed (critic suggested neither additional tasks nor retractions)"},
				{"repairRoundsUsed", repairRoundsUsed},
				{"maxRepairRounds", config_.maxRepairRounds},
			});
			stopped = true;
			break;
		}

		// --- 撤回を適用する ---
		// 追加より先に行う。分母から不要Taskを外してから再評価したいため。
		if (hasRetirements) {
			for (const auto& entry : obsolete) {
				if (!entry.is_number_integer()) continue;
				const TaskId obsoleteTaskId = static_cast<TaskId>(entry.get<std::int64_t>());
				evidenceBuilder.RequestRetireTask(obsoleteTaskId);
			}
			ReportProgress("Repair", Json::object({
				{"round", repairRoundsUsed + 1},
				{"retireRequested", obsolete.size()},
			}));
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

		// 追加Taskの件数上限は設けない。不足の数は要求が決めるものであり、
		// 上限を置くと「足りないまま出す」方向にしか働かない。
		for (const auto& add : additional) {
			if (!add.is_object()) {
				continue;
			}
			// 種別で弾かない。実在するToolを指しているかだけを見る
			// （SanitizeRepairDescriptionが検証する）。
			// 以前はRuntimeObservation/CodeSearch/Trace以外を捨てていたため、
			// Criticが「応答すればよい」と提案してもコードが握り潰していた。

			// 引数の不備は「計画の失敗」であって「調査の失敗」ではない。
			// 実行前に検証し、直せないものはTaskにしない。
			// こうしないと壊れたCommandが失敗Evidenceとして記録され、
			// カバレッジと失敗数を汚してrepairが状況を悪化させる（実機で観測）。
			std::string description = add.value("description", std::string());
			std::string rejectReason;
			if (!SanitizeRepairDescription(fullCatalogForRepair, &description, &rejectReason)) {
				ReportProgress("Repair", Json::object({
					{"round", repairRoundsUsed + 1},
					{"rejectedRepairTask", rejectReason},
				}));
				continue; // Taskを作らない＝Evidenceを汚さない
			}

			Json addSpec = Json::object();
			addSpec["taskId"] = "repair_" + std::to_string(repairRoundsUsed) + "_" + std::to_string(added);
			addSpec["description"] = description;
			addSpec["allowedTools"] = allToolNames; // 修復タスクはToken自体がObserve止まりなので全許可でも安全

			const TaskId storeTaskId = store_->CreateTask(sessionId, rootTaskId, "Repair", addSpec, 1);
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
	prompts::SetCurrentBuiltEvidence(builtJson);

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
		// ラウンド数を併記する。以前は打ち切りの理由と実際のラウンド消化数が
		// 一致せず（1/2ラウンドで抜けても"exhausted"と記録された）、
		// 停止理由の調査を毎回誤誘導していた。
		stopInfo = verdict.pass
			? Json::object({{"reason", "critic passed"}})
			: Json::object({
				{"reason", "repair rounds exhausted"},
				{"repairRoundsUsed", repairRoundsUsed},
				{"maxRepairRounds", config_.maxRepairRounds},
			});
	}

	// --- Synthesis ---
	ReportProgress("Synthesize", Json::object());
	std::string report;
	const Result synthesisResult =
		SynthesisAgent::Run(ctx, builtJson, rankedJson, stopInfo, &report);
	const bool completed = verdict.pass && static_cast<bool>(synthesisResult);
	if (!synthesisResult) {
		stopInfo = Json::object({{"reason", "synthesis validation failed: " + synthesisResult.error}});
	}

	store_->UpdateSessionState(sessionId, completed ? "Completed" : "Stopped");

	sessionResult.completed = completed;
	sessionResult.report = report;
	sessionResult.stopInfo = stopInfo;
	sessionResult.rankedHypotheses = rankedJson;
	// 判定の根拠（coverage / failedEvidenceCount / retiredTasks 等）を持ち帰る。
	sessionResult.builtEvidence = builtJson;
	return sessionResult;
}

} // namespace agentos
