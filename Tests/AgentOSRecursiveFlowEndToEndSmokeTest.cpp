// =======================================================================
//
// AgentOSRecursiveFlowEndToEndSmokeTest.cpp
//
// Root FlowがCreateChildFlowを実行し、子Flowが通常の
// Intake→Planner→Worker→Reason→Critic→Synthesisを完走した後、
// 親Flowへ短いTool Result/Evidenceとして戻ることを確認する。
//
// =======================================================================
#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include "AgentOS/Core/Command/CapabilitySet.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Command/CommandTypes.h"
#include "AgentOS/Core/Llm/ILlmBackend.h"
#include "AgentOS/Core/Orchestrator/Orchestrator.h"
#include "AgentOS/Core/Store/TaskStore.h"

using namespace agentos;

namespace {

class ProbeTool final : public ICommandExecutor {
public:
	ProbeTool() {
		descriptor_.name = "Probe";
		descriptor_.description = "子Flowが担当部分を確認するテスト用Tool";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object();
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json&) override {
		++executeCount;
		return CommandResult::Ok(Json::object({
			{"claim", "描画経路の入口はRenderer::Renderである"},
			{"symbol", "Renderer::Render"},
		}));
	}

	int executeCount = 0;

private:
	ToolDescriptor descriptor_;
};

class RecursiveFlowScriptBackend final : public ILlmBackend {
public:
	std::string Generate(
		const std::string& systemPrompt,
		const std::string& userPrompt,
		LlmGenerationStats* statsOut = nullptr) override {

		if (statsOut) {
			statsOut->promptChars = static_cast<std::int64_t>(systemPrompt.size() + userPrompt.size());
			statsOut->completionChars = 200;
			statsOut->stopReason = "completed";
		}

		if (systemPrompt.find("Command監視担当") != std::string::npos) {
			++monitorCalls;
			return "```json\n"
				"{\"approved\":true,\"isNarrower\":true,\"rootRelevant\":true,"
				"\"isDuplicate\":false,\"reason\":\"親の一部分へ限定されている\"}\n```";
		}

		if (systemPrompt.find("Intake担当") != std::string::npos) {
			++intakeCalls;
			if (userPrompt.find("描画経路だけを確認する") != std::string::npos) {
				return "```json\n"
					"{\"goal\":\"描画経路だけを確認する\","
					"\"resolvedRequest\":\"描画経路だけを確認する\","
					"\"turnRelation\":\"new\",\"referencedSessionIds\":[],"
					"\"symptoms\":[],\"constraints\":[],\"requiredCapabilities\":[\"Probe\"],"
					"\"unresolvedReferences\":[],\"targetKind\":\"concept\","
					"\"targetConcept\":null,\"resolvedEntityName\":null,"
					"\"requestType\":\"investigation\"}\n```";
			}
			return "```json\n"
				"{\"goal\":\"複数領域を調査する\","
				"\"resolvedRequest\":\"複数領域を調査する\","
				"\"turnRelation\":\"new\",\"referencedSessionIds\":[],"
				"\"symptoms\":[],\"constraints\":[],"
				"\"requiredCapabilities\":[\"CreateChildFlow\"],"
				"\"unresolvedReferences\":[],\"targetKind\":\"concept\","
				"\"targetConcept\":null,\"resolvedEntityName\":null,"
				"\"requestType\":\"investigation\"}\n```";
		}

		if (systemPrompt.find("Planner担当") != std::string::npos) {
			++plannerCalls;
			if (userPrompt.find("\"flowDepth\": 1") != std::string::npos) {
				return "```json\n"
					"{\"tasks\":[{\"taskId\":\"C1\","
					"\"description\":\"描画経路の入口を確認する\","
					"\"dependencies\":[],\"allowedTools\":[\"Probe\"],"
					"\"searchHints\":[]}]}\n```";
			}
			return "```json\n"
				"{\"tasks\":[{\"taskId\":\"T1\","
				"\"description\":\"大きな要求のうち描画経路を子Flowへ分ける\","
				"\"dependencies\":[],\"allowedTools\":[\"CreateChildFlow\"],"
				"\"searchHints\":[]}]}\n```";
		}

		if (systemPrompt.find("Task遂行用Tool呼び出し") != std::string::npos) {
			++workerPlanningCalls;
			return "```json\n"
				"{\"commands\":[{\"tool\":\"CreateChildFlow\",\"arguments\":{"
				"\"childTask\":\"描画経路だけを確認する\","
				"\"purpose\":\"Root Goalの描画領域を独立して確定する\","
				"\"successCondition\":\"描画入口のシンボルをEvidence付きで示せる\"}}]}\n```";
		}

		if (systemPrompt.find("Reasoning担当") != std::string::npos) {
			++reasonCalls;
			if (userPrompt.find("Tool:CreateChildFlow") != std::string::npos) {
				return "```json\n"
					"{\"hypotheses\":[{\"description\":\"子Flowで描画領域を確認できた\","
					"\"rubricBase\":0.9,\"supports\":[2],\"contradicts\":[],"
					"\"missingEvidence\":[]}]}\n```";
			}
			return "```json\n"
				"{\"hypotheses\":[{\"description\":\"描画入口はRenderer::Renderである\","
				"\"rubricBase\":0.9,\"supports\":[1],\"contradicts\":[],"
				"\"missingEvidence\":[]}]}\n```";
		}

		if (systemPrompt.find("Critic担当") != std::string::npos) {
			++criticCalls;
			return "```json\n"
				"{\"scores\":{\"evidenceCoverage\":1.0,\"contradictionHandling\":1.0,"
				"\"causalCompleteness\":1.0,\"testability\":1.0},"
				"\"failures\":[],\"goalSatisfied\":true,\"unmetAspects\":[],"
				"\"requestPatch\":null,\"additionalTasksSuggested\":[],"
				"\"obsoleteTasks\":[]}\n```";
		}

		if (systemPrompt.find("最終目的") != std::string::npos) {
			++synthesisCalls;
			if (userPrompt.find("Tool:CreateChildFlow") != std::string::npos) {
				return "```json\n{\"report\":\"親Flow完了: 子Flowの検証結果を統合した\"}\n```";
			}
			return "```json\n{\"report\":\"子Flow完了: 描画入口はRenderer::Render\"}\n```";
		}

		return "```json\n{}\n```";
	}

	int intakeCalls = 0;
	int plannerCalls = 0;
	int workerPlanningCalls = 0;
	int monitorCalls = 0;
	int reasonCalls = 0;
	int criticCalls = 0;
	int synthesisCalls = 0;
};

void RemoveDb(const std::string& path) {
	std::remove(path.c_str());
	std::remove((path + "-wal").c_str());
	std::remove((path + "-shm").c_str());
	std::remove((path + "-journal").c_str());
}

} // namespace

int main() {
	const std::string dbPath = "build/agentos_tests/recursive_flow_e2e.sqlite";
	RemoveDb(dbPath);

	TaskStore store;
	assert(store.Open(dbPath));

	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	auto probe = std::make_shared<ProbeTool>();
	pipeline.RegisterTool(probe);

	RecursiveFlowScriptBackend llm;
	OrchestratorConfig config;
	config.maxRepairRounds = 8;
	config.budget.maxDepth = 3;
	config.budget.maxToolCalls = 30;
	config.budget.maxLlmCalls = 30;
	config.budget.maxLlmChars = 500000;

	Orchestrator orchestrator(&llm, &pipeline, &store, &registry, config);
	const OrchestratorResult result = orchestrator.RunSession("複数領域を調査する");

	assert(result.completed);
	assert(result.report.find("親Flow完了") != std::string::npos);
	assert(probe->executeCount == 1);
	assert(llm.intakeCalls == 2);         // 親 + 子
	assert(llm.plannerCalls == 2);        // 親 + 子
	assert(llm.workerPlanningCalls == 1); // CreateChildFlowだけ。Probeは決定的生成。
	assert(llm.monitorCalls == 1);
	assert(llm.reasonCalls == 2);
	assert(llm.criticCalls == 2);
	assert(llm.synthesisCalls == 2);

	const auto audit = pipeline.GetAuditLog();
	assert(audit.size() == 2); // 子Probe + 親CreateChildFlow
	bool sawProbe = false;
	bool sawChildFlow = false;
	for (const auto& record : audit) {
		sawProbe = sawProbe || record.first.tool == "Probe";
		sawChildFlow = sawChildFlow || record.first.tool == "CreateChildFlow";
	}
	assert(sawProbe && sawChildFlow);

	RemoveDb(dbPath);
	std::cout << "AgentOSRecursiveFlowEndToEndSmokeTest: OK\n";
	return 0;
}
