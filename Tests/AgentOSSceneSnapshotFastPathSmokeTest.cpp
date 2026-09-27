// =======================================================================
//
// AgentOSSceneSnapshotFastPathSmokeTest.cpp
//
// 「現在のシーンの状態を報告して」を、Intake + 最終整形の2 LLM呼び出しだけで
// ListEntities / ListSystems / DescribeEntityへ安全に流すE2E回帰テスト。
//
// =======================================================================
#include "AgentOS/Core/Command/CapabilitySet.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Command/CommandTypes.h"
#include "AgentOS/Core/Llm/MockLlmBackend.h"
#include "AgentOS/Core/Orchestrator/Orchestrator.h"
#include "AgentOS/Core/Store/TaskStore.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

using namespace agentos;

namespace {

const char* kDbPath = "/tmp/agentos_scene_snapshot_fast_path_test.db";

void RemoveDb() {
	std::remove(kDbPath);
	std::remove((std::string(kDbPath) + "-wal").c_str());
	std::remove((std::string(kDbPath) + "-shm").c_str());
	std::remove((std::string(kDbPath) + "-journal").c_str());
}

class ListEntitiesTool final : public ICommandExecutor {
public:
	ListEntitiesTool() {
		descriptor_.name = "ListEntities";
		descriptor_.description = "現在のSceneに存在するEntity一覧を返す";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object();
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json&) override {
		++executeCount;
		return CommandResult::Ok(Json::object({
			{"claim", "生存Entityは2件"},
			{"entities", Json::array({
				Json::object({{"name", "Player"}, {"id", 1}}),
				Json::object({{"name", "MainCamera"}, {"id", 2}}),
			})},
		}));
	}

	int executeCount = 0;

private:
	ToolDescriptor descriptor_;
};

class ListSystemsTool final : public ICommandExecutor {
public:
	ListSystemsTool() {
		descriptor_.name = "ListSystems";
		descriptor_.description = "登録済みSystemTask一覧を返す";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object();
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json&) override {
		++executeCount;
		return CommandResult::Ok(Json::object({
			{"claim", "登録SystemはTransformSystemとRenderSystem"},
			{"systems", Json::array({"TransformSystem", "RenderSystem"})},
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
		descriptor_.description = "指定EntityのComponent概要を返す";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object({
			{"entityName", Json::object({{"type", "string"}, {"required", true}})},
		});
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json& arguments) override {
		++executeCount;
		const std::string name = arguments.value("entityName", std::string());
		if (name == "Player") {
			return CommandResult::Ok(Json::object({
				{"claim", "Player component snapshot"},
				{"name", name},
				{"components", Json::array({"Transform", "PlayerController"})},
			}));
		}
		if (name == "MainCamera") {
			return CommandResult::Ok(Json::object({
				{"claim", "MainCamera component snapshot"},
				{"name", name},
				{"components", Json::array({"Transform", "Camera"})},
			}));
		}
		return CommandResult::Fail(CommandStatus::ExecutionFailed, "unknown entity: " + name);
	}

	int executeCount = 0;

private:
	ToolDescriptor descriptor_;
};

} // namespace

int main() {
	std::cout << "=== AgentOS Scene Snapshot Fast Path Smoke Test ===\n";
	RemoveDb();

	TaskStore store;
	assert(store.Open(kDbPath));
	CapabilityRegistry capabilities;
	CommandPipeline pipeline(&capabilities);

	auto listEntities = std::make_shared<ListEntitiesTool>();
	auto listSystems = std::make_shared<ListSystemsTool>();
	auto describeEntity = std::make_shared<DescribeEntityTool>();
	pipeline.RegisterTool(listEntities);
	pipeline.RegisterTool(listSystems);
	pipeline.RegisterTool(describeEntity);

	MockLlmBackend llm;
	llm.AddRule("Intake担当",
		"```json\n"
		"{\"goal\":\"現在のシーンの状態を報告する\",\"symptoms\":[],\"constraints\":[],"
		"\"requiredCapabilities\":[\"ListEntities\",\"ListSystems\",\"DescribeEntity\"],"
		"\"requestType\":\"investigation\"}\n"
		"```");
	llm.AddRule("Synthesis担当",
		"```json\n"
		"{\"report\":\"現在のSceneにはPlayerとMainCameraが存在し、TransformSystemとRenderSystemが登録されています。\"}\n"
		"```");

	OrchestratorConfig config;
	config.maxRepairRounds = 0;
	Orchestrator orchestrator(&llm, &pipeline, &store, &capabilities, config);
	const OrchestratorResult result = orchestrator.RunSession("現在のシーンの状態を報告して");

	assert(result.completed);
	assert(result.stopInfo.value("reason", std::string()) == "critic passed");
	assert(result.report.find("Player") != std::string::npos);
	assert(result.report.find("MainCamera") != std::string::npos);
	assert(listEntities->executeCount == 1);
	assert(listSystems->executeCount == 1);
	assert(describeEntity->executeCount == 2);
	assert(llm.GetCalls().size() == 2); // Intake + Synthesis。Planner/Worker/Reason/Criticは0回。

	RemoveDb();
	std::cout << "=== ALL PASSED ===\n";
	return 0;
}
