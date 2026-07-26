// =======================================================================
//
// CommandMonitorAgent.h
//
// Workerが具体化したCommandをCommandPipelineへ渡す直前に検査する。
// 通常Toolは既存のSchema/Capability/Grounding検証へ任せ、再帰を発生させる
// CreateChildFlowだけはRoot Goal・親Task・祖先Taskと照合して監視する。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cctype>
#include <string>

#include "AgentContext.h"
#include "../Orchestrator/CreateChildFlowTool.h"
#include "../Orchestrator/FlowContext.h"

namespace agentos {

class CommandMonitorAgent {
public:
	static Result Review(AgentContext& ctx, const CommandRequest& request) {
		// まずは再帰を発生させるToolだけを重く監視する。
		// 他Toolも同じGateを通るが、Schema/Capability/Groundingへ委譲して追加LLMを使わない。
		if (request.tool != kCreateChildFlowToolName) return Result::Ok();

		const FlowContext& flow = CurrentFlowContext();
		if (!flow.active) {
			return Result::Fail("CreateChildFlow was requested without an active FlowContext");
		}
		// 子Flow自身のTaskは depth+1 になるため、次のFlow深度がmaxDepth以上なら作らない。
		if (flow.depth + 1 >= flow.maxDepth) {
			return Result::Fail(
				"CreateChildFlow would exceed max flow depth " + std::to_string(flow.maxDepth));
		}

		const std::string childTask = request.arguments.value("childTask", std::string());
		const std::string purpose = request.arguments.value("purpose", std::string());
		const std::string successCondition =
			request.arguments.value("successCondition", std::string());
		if (childTask.empty() || purpose.empty() || successCondition.empty()) {
			return Result::Fail("CreateChildFlow requires childTask, purpose and successCondition");
		}

		const std::string childKey = Normalize(childTask);
		if (childKey.empty()) return Result::Fail("childTask became empty after normalization");
		if (childKey == Normalize(flow.currentTask) ||
		    childKey == Normalize(flow.rootGoal) ||
		    childKey == Normalize(flow.rootResolvedRequest)) {
			return Result::Fail(
				"childTask is the same task or a direct restatement of the current/root task");
		}
		for (const std::string& ancestor : flow.ancestorTasks) {
			if (childKey == Normalize(ancestor)) {
				return Result::Fail("childTask duplicates an ancestor task");
			}
		}

		// 完全一致だけでは言い換えを検出できないため、CreateChildFlow時だけ
		// 小規模な監視LLMを使う。判定不能時は再帰を増やさない安全側へ倒す。
		PromptPair prompt;
		prompt.system =
			"あなたはAgentOSのCommand監視担当です。\n"
			"CreateChildFlowが本当に親Taskを小さく分解しているか判定してください。\n"
			"次をすべて満たす場合だけapproved=trueにします。\n"
			"- childTaskはcurrentTaskの単なる言い換えではなく、対象・範囲・確認事項のいずれかが狭い。\n"
			"- childTaskの完了がrootGoalの達成に必要である。\n"
			"- ancestorTasksと重複せず、successConditionだけで完了判定できる。\n"
			"- 通常Toolを1〜数回使うだけで現在Flow内に収まる仕事を不必要に子Flow化していない。\n"
			"出力は単一の```jsonフェンス内のJSONだけにしてください。\n"
			"出力スキーマ:\n"
			"{\"approved\": boolean, \"isNarrower\": boolean, \"rootRelevant\": boolean, "
			"\"isDuplicate\": boolean, \"reason\": string}\n/no_think\n";

		Json context = Json::object({
			{"rootGoal", flow.rootGoal},
			{"rootResolvedRequest", flow.rootResolvedRequest},
			{"currentTask", flow.currentTask},
			{"parentTask", flow.parentTask},
			{"ancestorTasks", flow.ancestorTasks},
			{"flowDepth", flow.depth},
			{"maxFlowDepth", flow.maxDepth},
			{"proposedCommand", request.arguments},
		});
		prompt.user = "監視対象:\n" + context.dump(2);

		Json raw;
		const Result llmResult = CallLlmJson(ctx, prompt, &raw);
		if (!llmResult) {
			return Result::Fail("child flow monitor LLM failed: " + llmResult.error);
		}
		const bool approved = raw.value("approved", false);
		const bool narrower = raw.value("isNarrower", false);
		const bool rootRelevant = raw.value("rootRelevant", false);
		const bool duplicate = raw.value("isDuplicate", true);
		if (!approved || !narrower || !rootRelevant || duplicate) {
			return Result::Fail(
				"child flow monitor rejected: " + raw.value("reason", std::string("unspecified")));
		}
		return Result::Ok();
	}

private:
	static std::string Normalize(const std::string& text) {
		std::string out;
		out.reserve(text.size());
		for (unsigned char ch : text) {
			if (ch < 128) {
				if (std::isalnum(ch) != 0 || ch == '_') {
					out.push_back(static_cast<char>(std::tolower(ch)));
				}
				continue;
			}
			// 日本語等のUTF-8バイト列はそのまま保持し、空白・ASCII句読点だけ落とす。
			out.push_back(static_cast<char>(ch));
		}
		return out;
	}
};

} // namespace agentos
