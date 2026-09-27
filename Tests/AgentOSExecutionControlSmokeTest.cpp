// =======================================================================
//
// AgentOSExecutionControlSmokeTest.cpp
//
// Planner fast path / dependency gating / argument grounding / usable
// Evidence coverage / Critic hard-failの回帰テスト。
//
// =======================================================================
#include "AgentOS/Core/Agents/AgentContext.h"
#include "AgentOS/Core/Agents/CriticAgent.h"
#include "AgentOS/Core/Agents/PlannerAgent.h"
#include "AgentOS/Core/Agents/RetrievalWorker.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Command/CommandSchema.h"
#include "AgentOS/Core/Command/CommandTypes.h"
#include "AgentOS/Core/Evidence/EvidenceBuilder.h"
#include "AgentOS/Core/Llm/MockLlmBackend.h"
#include "AgentOS/Core/Store/TaskStore.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace agentos;

namespace {

const char* kDbPath = "/tmp/agentos_execution_control_test.db";

void RemoveDb() {
	std::remove(kDbPath);
	std::remove((std::string(kDbPath) + "-wal").c_str());
	std::remove((std::string(kDbPath) + "-shm").c_str());
	std::remove((std::string(kDbPath) + "-journal").c_str());
}

class InspectEntityTool final : public ICommandExecutor {
public:
	InspectEntityTool() {
		descriptor_.name = "InspectEntity";
		descriptor_.description = "test entity lookup";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object({
			{"name", Json::object({{"type", "string"}, {"required", true}})},
		});
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json& arguments) override {
		++executeCount;
		const std::string name = arguments.value("name", std::string());
		return CommandResult::Ok(Json::object({
			{"found", name == "Player"},
			{"claim", "lookup: " + name},
		}));
	}

	int executeCount = 0;

private:
	ToolDescriptor descriptor_;
};

void TestRequiredStringRejectsBlank() {
	const Json schema = Json::object({
		{"name", Json::object({{"type", "string"}, {"required", true}})},
	});
	assert(!SchemaValidator::Validate(Json::object({{"name", ""}}), schema));
	assert(!SchemaValidator::Validate(Json::object({{"name", "   \t"}}), schema));
	assert(SchemaValidator::Validate(Json::object({{"name", "Player"}}), schema));
	std::puts("  - required string blank rejection: OK");
}

void TestDeterministicSceneSnapshotPlan() {
	MockLlmBackend llm;
	AgentContext ctx;
	ctx.llm = &llm;

	Json intake = Json::object({
		{"goal", "現在のシーンの状態を報告する"},
		{"symptoms", Json::array()},
		{"constraints", Json::array()},
		{"requiredCapabilities", Json::array({"ListEntities", "ListSystems"})},
		{"requestType", "investigation"},
	});
	Json catalog = Json::array({
		Json::object({{"name", "ListEntities"}}),
		Json::object({{"name", "ListSystems"}}),
		Json::object({{"name", "DescribeEntity"}}),
		Json::object({{"name", "StartWriteTrace"}}),
	});

	Json plan;
	Result result = PlannerAgent::Run(ctx, intake, catalog, &plan);
	assert(result);
	assert(llm.GetCalls().empty());
	assert(plan.value("route", std::string()) == "deterministic_scene_snapshot");
	assert(plan.at("tasks").size() == 4);
	for (const Json& task : plan.at("tasks")) {
		for (const Json& tool : task.value("allowedTools", Json::array())) {
			assert(tool.get<std::string>() != "StartWriteTrace");
		}
	}
	// Task種別(type)は廃止した。Taskの実体は「どのToolを使うか」。
	assert(plan.at("tasks")[0].at("allowedTools")[0].get<std::string>() == "ListEntities");
	// 旧Analysis相当（Workerを起動しないTask）は allowedTools が空であることで表す。
	assert(plan.at("tasks")[3].at("allowedTools").empty());
	std::puts("  - deterministic scene snapshot plan: OK");
}

void TestFailureEvidenceDoesNotCoverTask() {
	EvidenceBuilder builder;
	builder.MarkPlannedTask(1);
	builder.MarkPlannedTask(2);

	Evidence success;
	success.taskId = 1;
	success.claim = "entity list";
	success.payload = Json::object({{"entities", Json::array()}});
	success.provenance.sourceType = "Tool:ListEntities";
	builder.Add(success);

	Evidence failure;
	failure.taskId = 2;
	failure.claim = "command rejected";
	failure.payload = Json::object({{"failure", true}, {"error", "placeholder"}});
	failure.provenance.sourceType = "CommandValidationError";
	builder.Add(failure);

	const EvidenceBuilder::BuiltEvidence built = builder.Build();
	assert(std::abs(built.coverage - 0.5) < 1e-9);
	assert(built.usableEvidenceCount == 1);
	assert(built.failedEvidenceCount == 1);
	assert(built.tasksWithoutEvidence.size() == 1);
	assert(built.tasksWithoutEvidence[0] == 2);
	std::puts("  - failure evidence excluded from coverage: OK");
}

void TestDependencyFailureAndGrounding() {
	RemoveDb();
	TaskStore store;
	assert(store.Open(kDbPath));
	const SessionId session = store.CreateSession(Json::object({{"goal", "test"}}));
	const TaskId root = store.CreateTask(session, kInvalidId, "Intake", Json::object(), 0);

	auto tool = std::make_shared<InspectEntityTool>();
	CommandPipeline pipeline(nullptr);
	pipeline.RegisterTool(tool);

	const Json failedDepSpec = Json::object({{"taskId", "T1"}});
	const TaskId failedDep = store.CreateTask(session, root, "RuntimeObservation", failedDepSpec, 1);
	assert(store.UpdateTaskState(failedDep, TaskState::Running));
	assert(store.UpdateTaskState(failedDep, TaskState::Failed));

	const Json blockedSpec = Json::object({
		{"taskId", "T2"},
		{"type", "RuntimeObservation"},
		{"dependencies", Json::array({"T1"})},
		{"allowedTools", Json::array({"InspectEntity"})},
	});
	const TaskId blocked = store.CreateTask(session, root, "RuntimeObservation", blockedSpec, 1);
	assert(store.UpdateTaskState(blocked, TaskState::Running));

	MockLlmBackend blockedLlm;
	AgentContext blockedCtx;
	blockedCtx.llm = &blockedLlm;
	blockedCtx.pipeline = &pipeline;
	blockedCtx.store = &store;
	blockedCtx.sessionId = session;
	std::vector<Evidence> blockedEvidence;
	Json blockedSummary;
	Result blockedResult = RetrievalWorker::Run(blockedCtx, blocked, blockedSpec, &blockedEvidence, &blockedSummary);
	assert(!blockedResult);
	assert(blockedLlm.GetCalls().empty());
	assert(tool->executeCount == 0);
	assert(blockedEvidence.empty());
	assert(blockedSummary.value("skipped", false));

	const Json sourceSpec = Json::object({{"taskId", "T3"}});
	const TaskId source = store.CreateTask(session, root, "RuntimeObservation", sourceSpec, 1);
	assert(store.UpdateTaskState(source, TaskState::Running));
	Evidence entityEvidence;
	entityEvidence.taskId = source;
	entityEvidence.claim = "Entityを1件取得した";
	entityEvidence.payload = Json::object({
		{"entities", Json::array({Json::object({{"name", "Player"}})})},
	});
	entityEvidence.provenance.sourceType = "Tool:ListEntities";
	assert(store.AddEvidence(entityEvidence) != kInvalidId);
	assert(store.SetTaskResult(source, Json::object({{"executed", 1}})));
	assert(store.UpdateTaskState(source, TaskState::Succeeded));

	const Json targetSpec = Json::object({
		{"taskId", "T4"},
		{"type", "RuntimeObservation"},
		{"dependencies", Json::array({"T3"})},
		{"allowedTools", Json::array({"InspectEntity"})},
	});
	const TaskId target = store.CreateTask(session, root, "RuntimeObservation", targetSpec, 1);
	assert(store.UpdateTaskState(target, TaskState::Running));

	MockLlmBackend badLlm;
	badLlm.AddRule("Worker担当",
		"```json\n{\"commands\":[{\"tool\":\"InspectEntity\","
		"\"arguments\":{\"name\":\"主要な Entity\"}}]}\n```");
	AgentContext badCtx;
	badCtx.llm = &badLlm;
	badCtx.pipeline = &pipeline;
	badCtx.store = &store;
	badCtx.sessionId = session;
	std::vector<Evidence> badEvidence;
	Result badResult = RetrievalWorker::Run(badCtx, target, targetSpec, &badEvidence, nullptr);
	assert(!badResult);
	assert(tool->executeCount == 0);
	assert(badEvidence.size() == 1);
	assert(badEvidence[0].provenance.sourceType == "CommandValidationError");

	const Json goodSpec = Json::object({
		{"taskId", "T5"},
		{"type", "RuntimeObservation"},
		{"dependencies", Json::array({"T3"})},
		{"allowedTools", Json::array({"InspectEntity"})},
	});
	const TaskId goodTask = store.CreateTask(session, root, "RuntimeObservation", goodSpec, 1);
	assert(store.UpdateTaskState(goodTask, TaskState::Running));
	MockLlmBackend goodLlm;
	goodLlm.AddRule("Worker担当",
		"```json\n{\"commands\":[{\"tool\":\"InspectEntity\","
		"\"arguments\":{\"name\":\"Player\"}}]}\n```");
	AgentContext goodCtx;
	goodCtx.llm = &goodLlm;
	goodCtx.pipeline = &pipeline;
	goodCtx.store = &store;
	goodCtx.sessionId = session;
	std::vector<Evidence> goodEvidence;
	Result goodResult = RetrievalWorker::Run(goodCtx, goodTask, goodSpec, &goodEvidence, nullptr);
	assert(goodResult);
	assert(tool->executeCount == 1);
	assert(goodEvidence.size() == 1);
	assert(goodEvidence[0].provenance.sourceType == "Tool:InspectEntity");

	RemoveDb();
	std::puts("  - dependency gating and argument grounding: OK");
}

void TestCriticHardFailsOnFailedEvidence() {
	MockLlmBackend llm;
	llm.AddRule("Critic担当",
		"```json\n{\"scores\":{\"evidenceCoverage\":1.0,\"contradictionHandling\":1.0,"
		"\"causalCompleteness\":1.0,\"testability\":1.0},\"failures\":[],"
		"\"additionalTasksSuggested\":[]}\n```");
	AgentContext ctx;
	ctx.llm = &llm;

	const Json ranked = Json::object({
		{"hypotheses", Json::array({Json::object({{"confidence", 0.95}})})},
	});
	const Json built = Json::object({
		{"coverage", 1.0},
		{"usableEvidenceCount", 1},
		{"failedEvidenceCount", 1},
		{"tasksWithoutEvidence", Json::array()},
		{"contradictions", Json::array()},
		{"evidences", Json::array({
			Json::object({
				{"payload", Json::object({{"claim", "ok"}})},
				{"provenance", Json::object({{"sourceType", "Tool:ListEntities"}})},
			}),
			Json::object({
				{"payload", Json::object({{"failure", true}, {"error", "bad args"}})},
				{"provenance", Json::object({{"sourceType", "CommandValidationError"}})},
			}),
		})},
	});

	CriticVerdict verdict;
	assert(CriticAgent::Run(ctx, ranked, built, &verdict));
	assert(!verdict.pass);
	bool foundHardFail = false;
	for (const std::string& failure : verdict.failures) {
		if (failure.find("failed tool or command-validation evidence") != std::string::npos) {
			foundHardFail = true;
		}
	}
	assert(foundHardFail);
	std::puts("  - critic failed-evidence hard gate: OK");
}

} // namespace

int main() {
	std::cout << "=== AgentOS Execution Control Smoke Test ===\n";
	TestRequiredStringRejectsBlank();
	TestDeterministicSceneSnapshotPlan();
	TestFailureEvidenceDoesNotCoverTask();
	TestDependencyFailureAndGrounding();
	TestCriticHardFailsOnFailedEvidence();
	std::cout << "=== ALL PASSED ===\n";
	return 0;
}
