// =======================================================================
//
// AgentOSRecursiveFlowControlSmokeTest.cpp
//
// CreateChildFlow / FlowContext / Command Monitorの最小結合テスト。
//
// =======================================================================
#include <cassert>
#include <iostream>
#include <string>

#include "AgentOS/Core/Agents/AgentContext.h"
#include "AgentOS/Core/Agents/CommandMonitorAgent.h"
#include "AgentOS/Core/Command/CapabilitySet.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Orchestrator/CreateChildFlowTool.h"
#include "AgentOS/Core/Orchestrator/FlowContext.h"

using namespace agentos;

namespace {

CommandRequest MakeChildRequest(const CapabilityToken& token, const std::string& childTask) {
	CommandRequest request;
	request.taskId = 1;
	request.issuer = "RecursiveFlowTest";
	request.tool = kCreateChildFlowToolName;
	request.capability = token;
	request.arguments = Json::object({
		{"childTask", childTask},
		{"purpose", "Root Goalの一部分を独立して確認する"},
		{"successCondition", "対象部分についてEvidence付きの結論を返せる"},
	});
	return request;
}

void TestFlowContextScopeRestoresParent() {
	FlowContext root;
	root.active = true;
	root.sessionId = 10;
	root.rootSessionId = 10;
	root.depth = 0;
	root.maxDepth = 3;
	root.rootGoal = "AgentOS全体を改善する";
	root.rootResolvedRequest = root.rootGoal;
	root.currentTask = "Command経路を確認する";

	{
		ScopedFlowContext rootScope(root);
		assert(CurrentFlowContext().rootGoal == "AgentOS全体を改善する");
		{
			FlowContext child = CurrentFlowContext();
			child.sessionId = 11;
			child.parentSessionId = 10;
			child.depth = 1;
			child.parentTask = child.currentTask;
			child.currentTask = "Command Monitorだけを確認する";
			child.ancestorTasks.push_back(child.parentTask);
			ScopedFlowContext childScope(child);
			assert(CurrentFlowContext().rootGoal == root.rootGoal);
			assert(CurrentFlowContext().currentTask == "Command Monitorだけを確認する");
		}
		assert(CurrentFlowContext().sessionId == 10);
		assert(CurrentFlowContext().currentTask == "Command経路を確認する");
	}
	assert(!HasCurrentFlowContext());
}

void TestToolDescriptorAndRunner() {
	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	RegisterCreateChildFlowTool(pipeline);

	const Json catalog = pipeline.DescribeTools();
	bool found = false;
	for (const Json& tool : catalog) {
		if (tool.value("name", std::string()) != kCreateChildFlowToolName) continue;
		found = true;
		const Json schema = tool.value("argumentSchema", Json::object());
		assert(schema.contains("childTask"));
		assert(schema.contains("purpose"));
		assert(schema.contains("successCondition"));
		assert(!schema.contains("rootGoal")); // Root GoalはLLM引数ではなく実行環境から継承する。
	}
	assert(found);

	int runnerCalls = 0;
	ScopedChildFlowRunner runner([&runnerCalls](const Json& arguments) {
		++runnerCalls;
		return CommandResult::Ok(Json::object({
			{"claim", "child completed"},
			{"satisfied", true},
			{"childTask", arguments.at("childTask")},
		}));
	});

	pipeline.SetCommandMonitor([](const CommandRequest&) { return Result::Ok(); });
	const CapabilityToken token = registry.IssueToken(
		"RecursiveFlowTest", {kCreateChildFlowToolName}, PermissionLevel::Read);
	const CommandResult result = pipeline.Submit(MakeChildRequest(token, "Monitorだけを確認する"));
	assert(result.IsOk());
	assert(result.payload.value("satisfied", false));
	assert(runnerCalls == 1);
}

void TestCommonCommandMonitorRejectsBeforeExecute() {
	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	RegisterCreateChildFlowTool(pipeline);

	int runnerCalls = 0;
	ScopedChildFlowRunner runner([&runnerCalls](const Json&) {
		++runnerCalls;
		return CommandResult::Ok();
	});
	pipeline.SetCommandMonitor([](const CommandRequest&) {
		return Result::Fail("test rejection");
	});

	const CapabilityToken token = registry.IssueToken(
		"RecursiveFlowTest", {kCreateChildFlowToolName}, PermissionLevel::Read);
	const CommandResult result = pipeline.Submit(MakeChildRequest(token, "別の部分を確認する"));
	assert(result.status == CommandStatus::PreconditionRejected);
	assert(result.error.find("command monitor rejected") != std::string::npos);
	assert(runnerCalls == 0);
}

void TestSemanticMonitorRejectsExactParentRestatementWithoutLlm() {
	FlowContext flow;
	flow.active = true;
	flow.sessionId = 20;
	flow.rootSessionId = 20;
	flow.depth = 0;
	flow.maxDepth = 3;
	flow.rootGoal = "AgentOS全体を改善する";
	flow.rootResolvedRequest = flow.rootGoal;
	flow.currentTask = "Command経路を確認する";
	ScopedFlowContext flowScope(flow);

	AgentContext ctx; // 決定的拒否なのでLLMは不要。
	CommandRequest request;
	request.tool = kCreateChildFlowToolName;
	request.arguments = Json::object({
		{"childTask", "Command経路を確認する"},
		{"purpose", "確認する"},
		{"successCondition", "確認できる"},
	});
	const Result result = CommandMonitorAgent::Review(ctx, request);
	assert(!result);
	assert(result.error.find("same task") != std::string::npos);
}

} // namespace

int main() {
	TestFlowContextScopeRestoresParent();
	TestToolDescriptorAndRunner();
	TestCommonCommandMonitorRejectsBeforeExecute();
	TestSemanticMonitorRejectsExactParentRestatementWithoutLlm();
	std::cout << "AgentOSRecursiveFlowControlSmokeTest: OK\n";
	return 0;
}
