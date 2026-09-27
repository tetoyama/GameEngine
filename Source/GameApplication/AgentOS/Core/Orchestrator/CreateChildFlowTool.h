// =======================================================================
//
// CreateChildFlowTool.h
//
// 現在のFlowから、同じPlanner→Worker→Reason→Critic→Repairを持つ
// 子Flowを同期実行するTool。実行本体はOrchestratorがthread_local runnerとして
// 差し込むため、Tool自体はOrchestratorの寿命へ依存しない。
//
// =======================================================================
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "../Command/CommandPipeline.h"

namespace agentos {

inline constexpr const char* kCreateChildFlowToolName = "CreateChildFlow";
using ChildFlowRunner = std::function<CommandResult(const Json&)>;

inline ChildFlowRunner& ChildFlowRunnerRef() {
	static thread_local ChildFlowRunner runner;
	return runner;
}

class ScopedChildFlowRunner {
public:
	explicit ScopedChildFlowRunner(ChildFlowRunner runner)
		: previous_(std::move(ChildFlowRunnerRef())) {
		ChildFlowRunnerRef() = std::move(runner);
	}

	~ScopedChildFlowRunner() {
		ChildFlowRunnerRef() = std::move(previous_);
	}

	ScopedChildFlowRunner(const ScopedChildFlowRunner&) = delete;
	ScopedChildFlowRunner& operator=(const ScopedChildFlowRunner&) = delete;

private:
	ChildFlowRunner previous_;
};

class CreateChildFlowTool final : public ICommandExecutor {
public:
	CreateChildFlowTool()
		: descriptor_{
			kCreateChildFlowToolName,
			"現在のTaskが一つのFlowには大きすぎる場合に、より狭い独立Taskを同じAgentOS Flowで処理する。"
			"childTaskは親Taskの言い換えではなく、Root Goalの一部分だけを完了できる具体的な仕事にする。"
			"Root Goalは実行環境から自動継承されるため引数へ書かない。"
			"通常のCodeSearch等で現在Flow内に収まる場合は使わない。",
			PermissionLevel::Read,
			Json::object({
				{"childTask", Json::object({
					{"type", "string"},
					{"required", true},
					{"description", "親Taskより狭く、単独で完了判定できる子Task。"},
				})},
				{"purpose", Json::object({
					{"type", "string"},
					{"required", true},
					{"description", "この子TaskがRoot Goalのどの部分を進めるか。"},
				})},
				{"successCondition", Json::object({
					{"type", "string"},
					{"required", true},
					{"description", "子Flowを完了と判断できる具体的条件。"},
				})},
			})
		} {}

	const ToolDescriptor& Descriptor() const override { return descriptor_; }

	Result CheckPrecondition(const Json& arguments) override {
		for (const char* key : {"childTask", "purpose", "successCondition"}) {
			if (!arguments.contains(key) || !arguments.at(key).is_string() ||
			    arguments.at(key).get<std::string>().empty()) {
				return Result::Fail(std::string(key) + " は空でない文字列であること");
			}
		}
		if (!ChildFlowRunnerRef()) {
			return Result::Fail("CreateChildFlow runner is unavailable outside an active Flow");
		}
		return Result::Ok();
	}

	CommandResult Execute(const Json& arguments) override {
		ChildFlowRunner& runner = ChildFlowRunnerRef();
		if (!runner) {
			return CommandResult::Fail(
				CommandStatus::PreconditionRejected,
				"CreateChildFlow runner is unavailable outside an active Flow");
		}
		return runner(arguments);
	}

private:
	ToolDescriptor descriptor_;
};

inline void RegisterCreateChildFlowTool(CommandPipeline& pipeline) {
	// 同名ToolはCommandPipeline側で置換されるため、複数Orchestratorから安全に呼べる。
	pipeline.RegisterTool(std::make_shared<CreateChildFlowTool>());
}

} // namespace agentos
