#include <cassert>
#include <memory>

#include "AgentOS/Core/Command/CommandPipeline.h"

using namespace agentos;

namespace {

class ReadTool final : public ICommandExecutor {
public:
	ReadTool() {
		descriptor_.name = "ReadTool";
		descriptor_.requiredPermission = PermissionLevel::Read;
		descriptor_.argumentSchema = Json::object();
	}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }
	Result CheckPrecondition(const Json&) override { return Result::Ok(); }
	CommandResult Execute(const Json&) override {
		return CommandResult::Ok(Json::object({{"value", 1}}));
	}

private:
	ToolDescriptor descriptor_;
};

class CountingSink final : public IAuditSink {
public:
	explicit CountingSink(int* count) : count_(count) {}
	void OnCommand(const CommandRequest&, const CommandResult&) override {
		if(count_) ++(*count_);
	}

private:
	int* count_ = nullptr;
};

} // namespace

int main() {
	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);
	pipeline.RegisterTool(std::make_shared<ReadTool>());

	const CapabilityToken token = registry.IssueToken(
		"test",
		{"ReadTool"},
		PermissionLevel::Read
	);

	int firstSinkCount = 0;
	int replacementSinkCount = 0;
	pipeline.AddAuditSink(std::make_shared<CountingSink>(&firstSinkCount));
	assert(pipeline.HasAuditSinks());

	CommandRequest request;
	request.issuer = "test";
	request.tool = "ReadTool";
	request.arguments = Json::object();
	request.capability = token;
	assert(pipeline.Submit(request).IsOk());
	assert(firstSinkCount == 1);

	// 同じ動的型のSinkは追加ではなく置換される。
	pipeline.AddAuditSink(std::make_shared<CountingSink>(&replacementSinkCount));
	assert(pipeline.Submit(request).IsOk());
	assert(firstSinkCount == 1);
	assert(replacementSinkCount == 1);

	return 0;
}
