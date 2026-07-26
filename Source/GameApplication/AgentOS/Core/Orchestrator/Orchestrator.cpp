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
#include <utility>
#include <vector>

#include "CreateChildFlowTool.h"
#include "EarlyStopping.h"
#include "FlowContext.h"
#include "Supervisor.h"
#include "../Agents/AgentContext.h"
#include "../Agents/CommandMonitorAgent.h"
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
// Criticが提案する追加Taskの引数は、そのままではToolスキーマに合わないことがある。
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

	if (schema.is_object()) {
		Json cleaned = Json::object();
		for (auto it = arguments.begin(); it != arguments.end(); ++it) {
			if (schema.contains(it.key())) cleaned[it.key()] = it.value();
		}
		arguments = std::move(cleaned);
	}

	const Result validation = SchemaValidator::Validate(arguments, schema);
	if (!validation) {
		if (rejectReason) *rejectReason = validation.error;
		return false;
	}

	(*command)["arguments"] = std::move(arguments);
	return true;
}

bool SanitizeRepairDescription(const Json& toolCatalog, std::string* description, std::string* rejectReason) {
	if (description == nullptr) return true;

	static const std::string marker = "REPAIR_COMMAND ";
	const std::size_t pos = description->find(marker);
	if (pos == std::string::npos) return true;

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
// ---------------------------------
std::vector<Json> TopologicalOrder(const Json& tasks) {
	std::vector<Json> result;
	if (!tasks.is_array()) return result;

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
				if (!d.is_string()) continue;
				const std::string depId = d.get<std::string>();
				if (byId.count(depId) == 0) continue;
				dependents[depId].push_back(id);
				indegree[id] += 1;
			}
		}
	}

	std::vector<std::string> queue;
	for (const auto& id : order) {
		if (indegree[id] == 0) queue.push_back(id);
	}

	std::unordered_set<std::string> visited;
	std::size_t qi = 0;
	while (qi < queue.size()) {
		const std::string id = queue[qi++];
		if (visited.count(id) != 0) continue;
		visited.insert(id);
		result.push_back(byId[id]);
		for (const auto& dependent : dependents[id]) {
			indegree[dependent] -= 1;
			if (indegree[dependent] == 0) queue.push_back(dependent);
		}
	}

	for (const auto& id : order) {
		if (visited.count(id) == 0) result.push_back(byId[id]);
	}
	return result;
}

// 再帰Flowは同じCommandPipelineを使う。子Flow終了時に親のBudgetTrackerと
// Command Monitorを必ず戻さないと、親の残りTaskが子のスタック変数を参照する。
class ScopedBudgetTracker {
public:
	ScopedBudgetTracker(CommandPipeline* pipeline, BudgetTracker* tracker)
		: pipeline_(pipeline), previous_(pipeline ? pipeline->GetBudgetTracker() : nullptr) {
		if (pipeline_) pipeline_->SetBudgetTracker(tracker);
	}
	~ScopedBudgetTracker() {
		if (pipeline_) pipeline_->SetBudgetTracker(previous_);
	}
	ScopedBudgetTracker(const ScopedBudgetTracker&) = delete;
	ScopedBudgetTracker& operator=(const ScopedBudgetTracker&) = delete;

private:
	CommandPipeline* pipeline_ = nullptr;
	BudgetTracker* previous_ = nullptr;
};

class ScopedCommandMonitor {
public:
	ScopedCommandMonitor(CommandPipeline* pipeline, CommandPipeline::CommandMonitor monitor)
		: pipeline_(pipeline), previous_(pipeline ? pipeline->GetCommandMonitor() : CommandPipeline::CommandMonitor{}) {
		if (pipeline_) pipeline_->SetCommandMonitor(std::move(monitor));
	}
	~ScopedCommandMonitor() {
		if (pipeline_) pipeline_->SetCommandMonitor(std::move(previous_));
	}
	ScopedCommandMonitor(const ScopedCommandMonitor&) = delete;
	ScopedCommandMonitor& operator=(const ScopedCommandMonitor&) = delete;

private:
	CommandPipeline* pipeline_ = nullptr;
	CommandPipeline::CommandMonitor previous_;
};

class ScopedCurrentSession {
public:
	explicit ScopedCurrentSession(SessionId sessionId)
		: previous_(CurrentSessionId()) {
		SetCurrentSessionId(sessionId);
	}
	~ScopedCurrentSession() { SetCurrentSessionId(previous_); }
	ScopedCurrentSession(const ScopedCurrentSession&) = delete;
	ScopedCurrentSession& operator=(const ScopedCurrentSession&) = delete;

private:
	SessionId previous_ = kInvalidId;
};

// IntakeAgentはPrompt用thread_localを初期化する。子Flowが同じthreadで走るため、
// 子Flow終了時に親のRequest/Evidence/Tool Catalogを復元する。
class ScopedPromptRuntimeState {
public:
	ScopedPromptRuntimeState()
		: previousContext_(prompts::CurrentConversationRequestContext()),
		  previousToolCatalog_(prompts::CurrentToolCatalog()),
		  previousEvidence_(prompts::CurrentBuiltEvidence()) {}

	~ScopedPromptRuntimeState() {
		prompts::SetCurrentConversationRequestContext(
			previousContext_.value("conversation", Json::object()),
			previousContext_.value("intake", Json::object()));
		prompts::SetCurrentToolCatalog(previousToolCatalog_);
		prompts::SetCurrentBuiltEvidence(previousEvidence_);
	}

	ScopedPromptRuntimeState(const ScopedPromptRuntimeState&) = delete;
	ScopedPromptRuntimeState& operator=(const ScopedPromptRuntimeState&) = delete;

private:
	Json previousContext_;
	Json previousToolCatalog_;
	Json previousEvidence_;
};

Json FlowMetadata(const FlowContext& flow) {
	return Json::object({
		{"rootSessionId", flow.rootSessionId},
		{"parentSessionId", flow.parentSessionId},
		{"flowDepth", flow.depth},
		{"maxFlowDepth", flow.maxDepth},
		{"rootGoal", flow.rootGoal},
		{"rootResolvedRequest", flow.rootResolvedRequest},
		{"parentTask", flow.parentTask},
		{"currentTask", flow.currentTask},
		{"ancestorTasks", flow.ancestorTasks},
	});
}

Json CollectEvidenceIds(const Json& builtEvidence) {
	Json ids = Json::array();
	if (!builtEvidence.is_object() || !builtEvidence.contains("evidences") ||
	    !builtEvidence.at("evidences").is_array()) return ids;
	for (const Json& evidence : builtEvidence.at("evidences")) {
		if (evidence.is_object() && evidence.contains("id") &&
		    evidence.at("id").is_number_integer()) {
			ids.push_back(evidence.at("id"));
		}
	}
	return ids;
}

} // namespace

Orchestrator::Orchestrator(ILlmBackend* llm, CommandPipeline* pipeline, TaskStore* store,
                             CapabilityRegistry* capabilityRegistry, OrchestratorConfig config)
	: llm_(llm), pipeline_(pipeline), store_(store), capabilityRegistry_(capabilityRegistry), config_(config) {
	if (pipeline_) RegisterCreateChildFlowTool(*pipeline_);
}

void Orchestrator::SetProgressCallback(std::function<void(const std::string&, const Json&)> callback) {
	progressCallback_ = std::move(callback);
}

void Orchestrator::ReportProgress(const std::string& stage, const Json& detail) {
	if (!progressCallback_) return;
	Json enriched = detail.is_object() ? detail : Json::object({{"detail", detail}});
	if (HasCurrentFlowContext()) {
		const FlowContext& flow = CurrentFlowContext();
		enriched["flowDepth"] = flow.depth;
		enriched["rootSessionId"] = flow.rootSessionId;
		enriched["parentSessionId"] = flow.parentSessionId;
		enriched["currentTask"] = flow.currentTask;
	}
	progressCallback_(stage, enriched);
}

CapabilityToken Orchestrator::GetLastIssuedToken() const {
	return lastToken_;
}

OrchestratorResult Orchestrator::RunSession(const std::string& userRequest) {
	OrchestratorResult sessionResult;

	// 外側にFlowContextが無い場合だけRoot Flowを作る。CreateChildFlowからの再帰では
	// 呼び出し側がRoot Goalを継承した子Contextを既に積んでいる。
	std::unique_ptr<ScopedFlowContext> rootFlowGuard;
	if (!HasCurrentFlowContext()) {
		FlowContext root;
		root.active = true;
		root.depth = 0;
		root.maxDepth = config_.budget.maxDepth;
		root.rootGoal = userRequest;
		root.rootResolvedRequest = userRequest;
		root.currentTask = userRequest;
		rootFlowGuard = std::make_unique<ScopedFlowContext>(std::move(root));
	}
	FlowContext& flow = MutableCurrentFlowContext();

	ScopedPromptRuntimeState promptStateGuard;

	// Root FlowだけがBudgetを所有し、子Flowは同じTrackerを共有する。
	BudgetTracker ownedBudget(config_.budget);
	BudgetTracker* budgetTracker = flow.sharedBudget ? flow.sharedBudget : &ownedBudget;
	if (flow.sharedBudget == nullptr) flow.sharedBudget = budgetTracker;
	ScopedBudgetTracker budgetGuard(pipeline_, budgetTracker);

	const Json goalJson = Json::object({
		{"userRequest", userRequest},
		{"flow", FlowMetadata(flow)},
	});
	const SessionId sessionId = store_->CreateSession(goalJson);
	sessionResult.sessionId = sessionId;
	if (sessionId == kInvalidId) {
		sessionResult.completed = false;
		sessionResult.stopInfo = Json::object({{"reason", "CreateSession failed"}});
		sessionResult.report = "セッションを作成できませんでした。";
		return sessionResult;
	}

	flow.sessionId = sessionId;
	if (flow.rootSessionId == kInvalidId) flow.rootSessionId = sessionId;
	store_->UpdateSessionState(sessionId, "Running");

	const CapabilityToken token = capabilityRegistry_->IssueToken(
		"Orchestrator", {"*"}, PermissionLevel::Observe);
	lastToken_ = token;

	auto auditSink = std::make_shared<TaskStoreAuditSink>(store_);
	pipeline_->AddAuditSink(auditSink);

	AgentContext ctx;
	ctx.llm = llm_;
	ctx.pipeline = pipeline_;
	ctx.store = store_;
	ctx.budget = budgetTracker;
	ctx.token = token;
	ctx.sessionId = sessionId;

	ScopedCurrentSession sessionGuard(sessionId);
	ScopedCommandMonitor monitorGuard(
		pipeline_, [&ctx](const CommandRequest& request) {
			return CommandMonitorAgent::Review(ctx, request);
		});

	// CreateChildFlowは通常Toolと同様にCommandPipelineから実行される。
	// runnerはこのRunSession中だけ有効で、子Flowは同じOrchestratorと総Budgetを使う。
	ScopedChildFlowRunner childFlowRunnerGuard(
		[this](const Json& arguments) -> CommandResult {
			const FlowContext parent = CurrentFlowContext();
			if (!parent.active) {
				return CommandResult::Fail(
					CommandStatus::PreconditionRejected, "parent FlowContext is unavailable");
			}

			FlowContext child = parent;
			child.sessionId = kInvalidId;
			child.parentSessionId = parent.sessionId;
			child.depth = parent.depth + 1;
			child.parentTask = parent.currentTask;
			child.currentTask = arguments.value("childTask", std::string());
			if (!parent.currentTask.empty()) child.ancestorTasks.push_back(parent.currentTask);

			if (child.currentTask.empty() || child.depth >= child.maxDepth) {
				return CommandResult::Fail(
					CommandStatus::PreconditionRejected,
					"child Flow is empty or exceeds max depth");
			}

			ScopedFlowContext childContextGuard(std::move(child));
			const CapabilityToken parentToken = lastToken_;
			const OrchestratorResult childResult = RunSession(
				arguments.value("childTask", std::string()));
			lastToken_ = parentToken;

			const FlowContext& activeChild = CurrentFlowContext();
			const std::string report = prompts::Truncate(childResult.report, 5000);
			Json payload = Json::object({
				{"claim", childResult.completed
					? std::string("Child Flow completed: ") + report
					: std::string("Child Flow incomplete: ") + report},
				{"satisfied", childResult.completed},
				{"childSessionId", childResult.sessionId},
				{"rootSessionId", activeChild.rootSessionId},
				{"parentSessionId", activeChild.parentSessionId},
				{"flowDepth", activeChild.depth},
				{"childTask", arguments.value("childTask", std::string())},
				{"purpose", arguments.value("purpose", std::string())},
				{"successCondition", arguments.value("successCondition", std::string())},
				{"report", report},
				{"stopInfo", childResult.stopInfo},
				{"evidenceIds", CollectEvidenceIds(childResult.builtEvidence)},
			});
			return CommandResult::Ok(std::move(payload));
		});

	Supervisor supervisor(store_);

	// --- Intake（root task として記録） ---
	const TaskId rootTaskId = store_->CreateTask(
		sessionId, kInvalidId, "Intake", FlowMetadata(flow), flow.depth);
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

	if (flow.depth == 0) {
		// Root Goalは最初のIntakeで確定し、以後の子Flowでは変更しない。
		flow.rootGoal = intake.value("rootGoal", intake.value("goal", userRequest));
		flow.rootResolvedRequest = intake.value(
			"rootResolvedRequest", intake.value("resolvedRequest", userRequest));
		flow.currentTask = flow.rootResolvedRequest;
	} else {
		// 子Intakeが親目的を言い換えても、Root Goalだけは親Contextから上書きする。
		intake["rootGoal"] = flow.rootGoal;
		intake["rootResolvedRequest"] = flow.rootResolvedRequest;
		intake["goal"] = flow.currentTask;
		intake["resolvedRequest"] = flow.currentTask;
		intake["currentUserInput"] = flow.currentTask;
		intake["turnRelation"] = "new";
		intake["referencedSessionIds"] = Json::array();
		intake["conversationContext"] = Json::object();
		prompts::SetCurrentConversationRequestContext(Json::object(), intake);
	}
	intake["flow"] = FlowMetadata(flow);
	supervisor.CompleteTask(rootTaskId, intake);

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
	const int taskDepth = flow.depth + 1;

	for (const Json& taskSpec : orderedTasks) {
		const std::string planTaskId = taskSpec.value("taskId", std::string());
		const Json allowedTools = taskSpec.value("allowedTools", Json::array());
		const bool usesTool = allowedTools.is_array() && !allowedTools.empty();
		const std::string taskLabel = usesTool ? "Retrieval" : "Analysis";

		const TaskId storeTaskId = store_->CreateTask(
			sessionId, rootTaskId, taskLabel, taskSpec, taskDepth);
		ReportProgress("Retrieve", Json::object({{"taskId", storeTaskId}, {"planTaskId", planTaskId}}));

		Result startResult = supervisor.StartTask(storeTaskId);
		if (!startResult) continue;

		Result depthCheck = supervisor.CheckDepth(taskDepth, config_.budget);
		if (!depthCheck) {
			supervisor.FailTask(storeTaskId, depthCheck.error);
			continue;
		}

		if (!usesTool) {
			supervisor.CompleteTask(storeTaskId, Json::object({{"note", "handled by ReasoningAgent"}}));
			continue;
		}

		evidenceBuilder.MarkPlannedTask(storeTaskId);
		std::vector<Evidence> evidences;
		Json summary;
		Result workerResult;
		{
			ScopedCurrentFlowTask currentTaskGuard(
				taskSpec.value("description", std::string("Plan Task ")) + planTaskId);
			workerResult = RetrievalWorker::Run(
				ctx, storeTaskId, taskSpec, &evidences, &summary);
		}
		for (auto& evidence : evidences) evidenceBuilder.Add(evidence);

		if (!workerResult) supervisor.FailTask(storeTaskId, workerResult.error);
		else supervisor.CompleteTask(storeTaskId, summary);
	}

	// --- Evidence統合 ---
	EvidenceBuilder::BuiltEvidence built = evidenceBuilder.Build();
	Json builtJson = EvidenceBuilder::ToJson(built);
	prompts::SetCurrentBuiltEvidence(builtJson);

	// --- Reasoning ---
	ReportProgress("Reason", Json::object());
	LogicGraph graph;
	Json reasonRaw;
	ReasoningAgent::Run(ctx, builtJson, &graph, &reasonRaw);
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

		if (!hasAdditions && !hasRetirements) {
			stopInfo = Json::object({
				{"reason", "no remediation proposed (critic suggested neither additional tasks nor retractions)"},
				{"repairRoundsUsed", repairRoundsUsed},
				{"maxRepairRounds", config_.maxRepairRounds},
			});
			stopped = true;
			break;
		}

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
			for (const auto& tool : fullCatalogForRepair) {
				if (tool.contains("name")) allToolNames.push_back(tool.at("name"));
			}
		}

		for (const auto& add : additional) {
			if (!add.is_object()) continue;

			std::string description = add.value("description", std::string());
			std::string rejectReason;
			if (!SanitizeRepairDescription(fullCatalogForRepair, &description, &rejectReason)) {
				ReportProgress("Repair", Json::object({
					{"round", repairRoundsUsed + 1},
					{"rejectedRepairTask", rejectReason},
				}));
				continue;
			}

			Json addSpec = Json::object();
			addSpec["taskId"] = "repair_" + std::to_string(repairRoundsUsed) + "_" + std::to_string(added);
			addSpec["description"] = description;
			addSpec["allowedTools"] = allToolNames;

			const TaskId storeTaskId = store_->CreateTask(
				sessionId, rootTaskId, "Repair", addSpec, taskDepth);
			supervisor.StartTask(storeTaskId);
			evidenceBuilder.MarkPlannedTask(storeTaskId);

			std::vector<Evidence> evidences;
			Json summary;
			Result workerResult;
			{
				ScopedCurrentFlowTask currentTaskGuard(description);
				workerResult = RetrievalWorker::Run(
					ctx, storeTaskId, addSpec, &evidences, &summary);
			}
			for (auto& evidence : evidences) evidenceBuilder.Add(evidence);
			if (!workerResult) supervisor.FailTask(storeTaskId, workerResult.error);
			else supervisor.CompleteTask(storeTaskId, summary);
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
		const int newEvidenceCount =
			static_cast<int>(built.evidences.size()) - previousEvidenceCount;
		const bool sameFailureRepeated =
			!verdict.failures.empty() && verdict.failures == previousFailures;

		earlyStopping.RecordRound(
			newEvidenceCount, newTop, static_cast<int>(built.contradictions.size()),
			sameFailureRepeated);

		previousEvidenceCount = static_cast<int>(built.evidences.size());
		previousFailures = verdict.failures;
		++repairRoundsUsed;

		const EarlyStopping::StopDecision decision = earlyStopping.Evaluate(*budgetTracker);
		if (decision.stop) {
			stopInfo = Json::object({{"reason", "early stopping: " + decision.reason}});
			stopped = true;
			break;
		}
	}

	if (!stopped) {
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
	sessionResult.builtEvidence = builtJson;
	return sessionResult;
}

} // namespace agentos
