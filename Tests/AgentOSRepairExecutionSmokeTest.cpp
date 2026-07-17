// =======================================================================
//
// AgentOSRepairExecutionSmokeTest.cpp
//
// 誤ったEntity名によるUnsatisfiedからRequest Revisionを作り、Criticの
// explicit repair commandsを実行して新Evidenceで通過するE2Eテスト。
//
// =======================================================================
#include "AgentOS/Core/Command/CapabilitySet.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Command/CommandTypes.h"
#include "AgentOS/Core/Llm/MockLlmBackend.h"
#include "AgentOS/Core/Llm/PromptTemplates.h"
#include "AgentOS/Core/Orchestrator/Orchestrator.h"
#include "AgentOS/Core/Store/TaskStore.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

using namespace agentos;

namespace {

const char* kDbPath = "/tmp/agentos_repair_execution_test.db";

void RemoveDb() {
	std::remove(kDbPath);
	std::remove((std::string(kDbPath) + "-wal").c_str());
	std::remove((std::string(kDbPath) + "-shm").c_str());
	std::remove((std::string(kDbPath) + "-journal").c_str());
}

class FindEntityTool final : public ICommandExecutor {
public:
	FindEntityTool() {
		descriptor_.name = "FindEntityByName";
		descriptor_.description = "Entity名を検索する";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object({
			{"name", Json::object({{"type", "string"}, {"required", true}})},
		});
	}
	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json& arguments) override {
		++executeCount;
		return CommandResult::Ok(Json::object({
			{"found", false},
			{"query", arguments.value("name", std::string())},
			{"claim", "指定名のEntityは見つからなかった"},
		}));
	}
	int executeCount = 0;
private:
	ToolDescriptor descriptor_;
};

class ListEntitiesTool final : public ICommandExecutor {
public:
	ListEntitiesTool() {
		descriptor_.name = "ListEntities";
		descriptor_.description = "Entity一覧を取得する";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object();
	}
	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json&) override {
		++executeCount;
		return CommandResult::Ok(Json::object({
			{"claim", "Field Entityが存在する"},
			{"entities", Json::array({Json::object({{"name", "Field"}, {"id", 1}})})},
		}));
	}
	int executeCount = 0;
private:
	ToolDescriptor descriptor_;
};

class DescribeEntityTool final : public ICommandExecutor {
public:
	DescribeEntityTool() {
		descriptor_.name = "DescribeEntity";
		descriptor_.description = "EntityのComponent一覧を取得する";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object({
			{"entityName", Json::object({{"type", "string"}, {"required", true}})},
		});
	}
	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json& arguments) override {
		++executeCount;
		assert(arguments.value("entityName", std::string()) == "Field");
		return CommandResult::Ok(Json::object({
			{"claim", "FieldのComponentを取得した"},
			{"name", "Field"},
			{"components", Json::array({"Transform", "MeshRenderer", "Collider"})},
		}));
	}
	int executeCount = 0;
private:
	ToolDescriptor descriptor_;
};

} // namespace

int main() {
	std::cout << "=== AgentOS Repair Execution Smoke Test ===\n";
	RemoveDb();

	TaskStore store;
	assert(store.Open(kDbPath));
	CapabilityRegistry capabilities;
	CommandPipeline pipeline(&capabilities);
	auto find = std::make_shared<FindEntityTool>();
	auto list = std::make_shared<ListEntitiesTool>();
	auto describe = std::make_shared<DescribeEntityTool>();
	pipeline.RegisterTool(find);
	pipeline.RegisterTool(list);
	pipeline.RegisterTool(describe);

	MockLlmBackend llm;
	llm.EnqueueResponse(
		"```json\n"
		"{\"goal\":\"Field EntityのComponentを取得する\","
		"\"resolvedRequest\":\"Field Entityに設定されているComponent一覧を取得する\","
		"\"turnRelation\":\"new\",\"referencedSessionIds\":[],\"symptoms\":[],"
		"\"constraints\":[],\"requiredCapabilities\":[\"FindEntityByName\"],"
		"\"unresolvedReferences\":[],\"requestType\":\"investigation\"}\n"
		"```");
	llm.EnqueueResponse(
		"```json\n"
		"{\"tasks\":[{\"taskId\":\"T1\",\"type\":\"RuntimeObservation\","
		"\"description\":\"Field Entityを検索する\",\"dependencies\":[],"
		"\"allowedTools\":[\"FindEntityByName\"],\"searchHints\":[\"Field Entity\"]}]}\n"
		"```");
	llm.EnqueueResponse(
		"```json\n"
		"{\"commands\":[{\"tool\":\"FindEntityByName\","
		"\"arguments\":{\"name\":\"Field Entity\"}}]}\n"
		"```");
	llm.EnqueueResponse(
		"```json\n"
		"{\"hypotheses\":[{\"description\":\"対象名が誤っている\","
		"\"rubricBase\":0.5,\"supports\":[1],\"contradicts\":[],"
		"\"missingEvidence\":[\"正しいEntity名\"]}]}\n"
		"```");
	llm.EnqueueResponse(
		"```json\n"
		"{\"scores\":{\"evidenceCoverage\":0.0,\"contradictionHandling\":1.0,"
		"\"causalCompleteness\":0.2,\"testability\":0.8},"
		"\"failures\":[\"Field Entityは実在名ではない\"],"
		"\"requestPatch\":{\"goal\":\"FieldのComponentを取得する\","
		"\"resolvedRequest\":\"Entity名Fieldに設定されているComponent一覧を取得する\","
		"\"constraints\":[],\"reason\":\"検索失敗から名称を修正\"},"
		"\"additionalTasksSuggested\":["
		"{\"type\":\"correction\",\"description\":\"正確なEntity一覧を取得する\","
		"\"tool\":\"ListEntities\",\"arguments\":{}},"
		"{\"type\":\"RuntimeObservation\",\"description\":\"Fieldを詳細取得する\","
		"\"tool\":\"DescribeEntity\",\"arguments\":{\"entityName\":\"Field\"}}]}\n"
		"```");
	llm.EnqueueResponse(
		"```json\n"
		"{\"hypotheses\":[{\"description\":\"FieldにはTransform、MeshRenderer、Colliderがある\","
		"\"rubricBase\":1.0,\"supports\":[2,3],\"contradicts\":[],"
		"\"missingEvidence\":[]}]}\n"
		"```");
	llm.EnqueueResponse(
		"```json\n"
		"{\"scores\":{\"evidenceCoverage\":1.0,\"contradictionHandling\":1.0,"
		"\"causalCompleteness\":1.0,\"testability\":1.0},"
		"\"failures\":[],\"requestPatch\":null,\"additionalTasksSuggested\":[]}\n"
		"```");
	llm.EnqueueResponse(
		"```json\n"
		"{\"report\":\"FieldにはTransform、MeshRenderer、Colliderが設定されています。\"}\n"
		"```");

	OrchestratorConfig config;
	config.maxRepairRounds = 2;
	config.budget.maxLlmCalls = 16;
	config.budget.maxLlmChars = 500000;
	Orchestrator orchestrator(&llm, &pipeline, &store, &capabilities, config);
	const OrchestratorResult result = orchestrator.RunSession(
		"Fieldになんのコンポーネントがあるの？");

	assert(result.completed);
	assert(result.stopInfo.value("reason", std::string()) == "critic passed");
	assert(result.report.find("MeshRenderer") != std::string::npos);
	assert(find->executeCount == 1);
	assert(list->executeCount == 1);
	assert(describe->executeCount == 1);
	assert(prompts::CurrentRequestRevision() == 1);
	assert(prompts::CurrentResolvedRequest().find("Entity名Field") != std::string::npos);

	prompts::ClearCurrentConversationRequestContext();
	RemoveDb();
	std::cout << "=== ALL PASSED ===\n";
	return 0;
}
