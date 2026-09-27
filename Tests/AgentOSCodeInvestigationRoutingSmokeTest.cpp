// =======================================================================
//
// AgentOSCodeInvestigationRoutingSmokeTest.cpp
//
// 静的コード調査がRuntime Entity系Toolへ逸れず、定義取得と参照元完全走査へ
// 決定的にルーティングされることを検証する。
//
// =======================================================================
#include "AgentOS/Core/Agents/PlannerAgent.h"
#include "AgentOS/Core/CodeIndex/CodeIndexService.h"
#include "AgentOS/Core/CodeIndex/CodeSearchTool.h"
#include "AgentOS/Core/Command/CapabilitySet.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Command/CommandTypes.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace agentos;

namespace {

Json ToolCatalog() {
	return Json::array({
		Json::object({{"name", "CodeSearch"}}),
		Json::object({{"name", "GetSymbolInfo"}}),
		Json::object({{"name", "FindCodeReferences"}}),
		Json::object({{"name", "ListEntities"}}),
		Json::object({{"name", "ResolveEntity"}}),
	});
}

bool PlanUsesTool(const Json& plan, const std::string& toolName) {
	for(const Json& task : plan.value("tasks", Json::array())) {
		for(const Json& tool : task.value("allowedTools", Json::array())) {
			if(tool.is_string() && tool.get<std::string>() == toolName) return true;
		}
	}
	return false;
}

void TestCallerRouting() {
	AgentContext ctx;
	Json plan;
	const Json intake = Json::object({
		{"goal", "SqliteDb::Prepareの呼び出し元を調べる"},
		{"resolvedRequest", "SqliteDb::Prepareを呼び出している箇所をすべて探して"},
	});
	assert(PlannerAgent::Run(ctx, intake, ToolCatalog(), &plan));
	assert(plan.value("route", std::string()) == "deterministic_code_investigation");
	assert(PlanUsesTool(plan, "GetSymbolInfo"));
	assert(PlanUsesTool(plan, "FindCodeReferences"));
	assert(!PlanUsesTool(plan, "ListEntities"));
	assert(!PlanUsesTool(plan, "ResolveEntity"));
}

void TestExactImplementationRouting() {
	AgentContext ctx;
	Json plan;
	const Json intake = Json::object({
		{"goal", "実装を表示する"},
		{"resolvedRequest", "SqliteDb::Prepareの実装を文字列リテラルも変更せずそのまま表示して"},
	});
	assert(PlannerAgent::Run(ctx, intake, ToolCatalog(), &plan));
	assert(plan.value("route", std::string()) == "deterministic_code_investigation");
	assert(PlanUsesTool(plan, "GetSymbolInfo"));
	assert(!PlanUsesTool(plan, "ListEntities"));
}

void TestPostconditionRouting() {
	AgentContext ctx;
	Json plan;
	const Json intake = Json::object({
		{"goal", "postcondition機構の実装有無を確認する"},
		{"resolvedRequest", "AgentOSにはコマンド実行後のpostcondition機構が実装されている？"},
	});
	assert(PlannerAgent::Run(ctx, intake, ToolCatalog(), &plan));
	assert(plan.value("route", std::string()) == "deterministic_code_investigation");
	assert(PlanUsesTool(plan, "FindCodeReferences"));
	assert(!PlanUsesTool(plan, "ListEntities"));
}

void TestReferenceTool() {
	const std::filesystem::path root = "/tmp/agentos_code_reference_test";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root / "Source");
	{
		std::ofstream out(root / "Source" / "Sample.cpp");
		out << "void Target() {}\n";
		out << "void CallerA() { Target(); }\n";
		out << "void CallerB() { Target(); }\n";
	}

	CodeIndexService service;
	CodeIndexServiceContext context;
	context.sourceRoot = (root / "Source").string();
	context.databasePath = (root / "unused.db").string();
	context.buildOnStart = false;
	assert(service.Initialize(context));

	CapabilityRegistry capabilities;
	CommandPipeline pipeline(&capabilities);
	RegisterCodeSearchTool(pipeline, service);
	const CapabilityToken token = capabilities.IssueToken(
		"ReferenceTest", {"FindCodeReferences"}, PermissionLevel::Read);

	CommandRequest request;
	request.taskId = 1;
	request.issuer = "ReferenceTest";
	request.tool = "FindCodeReferences";
	request.arguments = Json::object({{"query", "Target("}, {"topK", 100}});
	request.capability = token;
	const CommandResult found = pipeline.Submit(request);
	assert(found.IsOk());
	assert(found.payload.value("complete_scan", false));
	assert(found.payload.value("count", 0) == 3);
	assert(found.payload.value("returned", 0) == 3);

	request.arguments = Json::object({{"query", "DefinitelyMissing("}});
	const CommandResult missing = pipeline.Submit(request);
	assert(missing.IsOk());
	assert(missing.payload.value("complete_scan", false));
	assert(missing.payload.value("count", 1) == 0);

	capabilities.Revoke(token);
	service.Shutdown();
	std::filesystem::remove_all(root);
}

} // namespace

int main() {
	std::cout << "=== AgentOS Code Investigation Routing Smoke Test ===\n";
	TestCallerRouting();
	TestExactImplementationRouting();
	TestPostconditionRouting();
	TestReferenceTool();
	std::cout << "=== ALL PASSED ===\n";
	return 0;
}
