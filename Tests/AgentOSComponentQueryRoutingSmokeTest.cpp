#include <cassert>
#include <iostream>
#include <memory>

#include "AgentOS/Core/Command/CapabilitySet.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Logic/ComponentQueryRouting.h"

using namespace agentos;

namespace {

class FakeReadComponentTool : public ICommandExecutor {
public:
	FakeReadComponentTool() {
		descriptor_.name = "ReadComponent";
		descriptor_.description = "test";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object({
			{"entityName", Json::object({
				{"type", "string"},
				{"required", true},
			})},
			{"component", Json::object({
				{"type", "string"},
				{"required", true},
			})},
		});
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }

	CommandResult Execute(const Json& arguments) override {
		return CommandResult::Ok(arguments);
	}

private:
	ToolDescriptor descriptor_;
};

void TestRoutes() {
	{
		const auto route = componentquery::Resolve(
			"Fieldになんのコンポーネントがあるの？");
		assert(route.tool == "DescribeEntity");
		assert(route.arguments.at("entityName") == "Field");
	}
	{
		const auto route = componentquery::Resolve("Entity 'Light' にも？");
		assert(route.tool == "DescribeEntity");
		assert(route.arguments.at("entityName") == "Light");
	}
	{
		const auto route = componentquery::Resolve(
			"LightComponentの設定を教えて");
		assert(route.tool == "ReadComponent");
		assert(route.arguments.at("entityName") == "Light");
		assert(route.arguments.at("component") == "LightComponent");
	}
	{
		const auto route = componentquery::Resolve(
			"ライトコンポーネントとかもない状態？");
		assert(route.tool == "ReadComponent");
		assert(route.arguments.at("entityName") == "Light");
		assert(route.arguments.at("component") == "LightComponent");
	}
	{
		// Scene全体の要求はListEntitiesだけに短絡せず、
		// PlannerのScene Snapshot経路へ渡す。
		const auto route = componentquery::Resolve("今のシーンの状況を教えて");
		assert(!route.IsValid());
	}
}

void TestDeterministicReplies() {
	componentquery::Route route{
		"DescribeEntity",
		Json::object({{"entityName", "Field"}}),
	};
	Json payload = Json::object({
		{"components", Json::array({
			Json::object({{"component", "NameComponent"}}),
			Json::object({{"component", "TransformComponent"}}),
		})},
	});

	const std::string reply = componentquery::BuildReply(route, payload);
	assert(reply.find("2件") != std::string::npos);
	assert(reply.find("NameComponent") != std::string::npos);
	assert(reply.find("登録されていない") == std::string::npos);

	route = componentquery::Route{
		"ReadComponent",
		Json::object({
			{"entityName", "Light"},
			{"component", "LightComponent"},
		}),
	};
	payload = Json::object({
		{"component", "LightComponent"},
		{"value", Json::object({
			{"Enable", 1},
			{"CastShadow", 1},
		})},
	});

	const std::string valueReply = componentquery::BuildReply(route, payload);
	assert(valueReply.find("Enable") != std::string::npos);
	assert(valueReply.find("CastShadow") != std::string::npos);
	assert(componentquery::PayloadSatisfied(payload));
	assert(!componentquery::PayloadSatisfied(
		Json::object({{"error", "missing"}})));
}

void TestRepairArgumentAliases() {
	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	pipeline.RegisterTool(std::make_shared<FakeReadComponentTool>());
	CapabilityToken token = registry.IssueToken(
		"RepairWorker",
		{"ReadComponent"},
		PermissionLevel::Read);

	CommandRequest request;
	request.issuer = "RepairWorker";
	request.tool = "ReadComponent";
	request.arguments = Json::object({
		{"entityId", "Light"},
		{"componentName", "LightComponent"},
	});
	request.capability = token;

	const CommandResult result = pipeline.Submit(request);
	assert(result.IsOk());
	assert(result.payload.at("entityName") == "Light");
	assert(result.payload.at("component") == "LightComponent");
	assert(!result.payload.contains("entityId"));
	assert(!result.payload.contains("componentName"));
}

} // namespace

int main() {
	TestRoutes();
	TestDeterministicReplies();
	TestRepairArgumentAliases();
	std::cout << "AgentOSComponentQueryRoutingSmokeTest: PASS\n";
	return 0;
}
