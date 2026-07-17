// =======================================================================
//
// RetrievalWorker.cpp
//
// =======================================================================
#include "RetrievalWorker.h"

#include <algorithm>
#include <string>
#include <unordered_set>

namespace agentos {

namespace {

double ExtractConfidenceHint(const Json& payload) {
	if (payload.is_object() && payload.contains("confidenceHint") && payload.at("confidenceHint").is_number()) {
		return payload.at("confidenceHint").get<double>();
	}
	return 1.0;
}

std::string ExtractClaim(const Json& payload, const std::string& toolName) {
	if (payload.is_object() && payload.contains("claim") && payload.at("claim").is_string()) {
		return payload.at("claim").get<std::string>();
	}
	return toolName + " result";
}

std::string ExtractPlanTaskId(const Json& taskSpec) {
	if (taskSpec.is_object() && taskSpec.contains("taskId") && taskSpec.at("taskId").is_string()) {
		return taskSpec.at("taskId").get<std::string>();
	}
	return "?";
}

} // namespace

Result RetrievalWorker::Run(AgentContext& ctx, TaskId storeTaskId, const Json& taskSpec,
                             std::vector<Evidence>* evidenceOut, Json* summaryOut) {
	if (evidenceOut == nullptr) {
		return Result::Fail("RetrievalWorker: evidenceOut is null");
	}

	// --- taskSpec.allowedToolsのみに絞ったTool catalogを作る ---
	std::unordered_set<std::string> allowed;
	if (taskSpec.is_object() && taskSpec.contains("allowedTools") && taskSpec.at("allowedTools").is_array()) {
		for (const auto& t : taskSpec.at("allowedTools")) {
			if (t.is_string()) {
				allowed.insert(t.get<std::string>());
			}
		}
	}

	Json filteredCatalog = Json::array();
	const Json fullCatalog = ctx.pipeline->DescribeTools();
	if (fullCatalog.is_array()) {
		for (const auto& entry : fullCatalog) {
			if (entry.is_object() && entry.contains("name") && entry.at("name").is_string() &&
			    allowed.count(entry.at("name").get<std::string>()) != 0) {
				filteredCatalog.push_back(entry);
			}
		}
	}

	const PromptPair prompt = prompts::GenerateQueries(taskSpec, filteredCatalog);
	Json raw;
	Result callResult = CallLlmJson(ctx, prompt, &raw);
	if (!callResult) {
		return Result::Fail("RetrievalWorker: query generation failed: " + callResult.error);
	}

	Json commands = Json::array();
	if (raw.is_object() && raw.contains("commands") && raw.at("commands").is_array()) {
		commands = raw.at("commands");
	}

	int executed = 0;
	int failed = 0;

	const std::size_t limit = std::min<std::size_t>(commands.size(), 5);
	for (std::size_t i = 0; i < limit; ++i) {
		const Json& cmd = commands[i];
		if (!cmd.is_object() || !cmd.contains("tool") || !cmd.at("tool").is_string()) {
			continue; // 提案が不正な形式ならスキップ（Workerは結論を出さないので黙って無視）
		}
		const std::string toolName = cmd.at("tool").get<std::string>();
		const Json arguments = cmd.value("arguments", Json::object());

		CommandRequest request;
		request.taskId = storeTaskId;
		request.issuer = "RetrievalWorker";
		request.tool = toolName;
		request.arguments = arguments;
		request.capability = ctx.token;

		const CommandResult result = ctx.pipeline->Submit(request);

		Evidence evidence;
		evidence.taskId = storeTaskId;
		evidence.provenance.session = "session_" + std::to_string(ctx.sessionId);

		if (result.IsOk()) {
			++executed;
			evidence.claim = ExtractClaim(result.payload, toolName);
			evidence.payload = result.payload;
			evidence.provenance.sourceType = "Tool:" + toolName;
			evidence.provenance.sourceUri = toolName;
			evidence.confidence = ExtractConfidenceHint(result.payload);
		} else {
			// 失敗も隠さずEvidenceとして報告する（上位層が判断できるように）。
			++failed;
			evidence.claim = "Tool " + toolName + " failed: " + result.error;
			evidence.payload = Json::object({{"status", ToString(result.status)}});
			evidence.provenance.sourceType = "ToolError";
			evidence.provenance.sourceUri = toolName;
			evidence.confidence = 0.3;
		}

		const EvidenceId id = ctx.store->AddEvidence(evidence);
		evidence.id = id;
		evidenceOut->push_back(evidence);
	}

	if (summaryOut != nullptr) {
		Json summary = Json::object();
		summary["executed"] = executed;
		summary["failed"] = failed;
		summary["coverageNote"] = "task '" + ExtractPlanTaskId(taskSpec) + "': executed=" +
			std::to_string(executed) + " failed=" + std::to_string(failed);
		*summaryOut = std::move(summary);
	}

	return Result::Ok();
}

} // namespace agentos
