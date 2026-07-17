// =======================================================================
//
// RetrievalWorker.cpp
//
// =======================================================================
#include "RetrievalWorker.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

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

std::string Trim(std::string value) {
	auto isNotSpace = [](unsigned char ch) { return std::isspace(ch) == 0; };
	value.erase(value.begin(), std::find_if(value.begin(), value.end(), isNotSpace));
	value.erase(std::find_if(value.rbegin(), value.rend(), isNotSpace).base(), value.end());
	return value;
}

std::string LowerAscii(std::string value) {
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return value;
}

bool IsPlaceholderString(const std::string& raw) {
	const std::string value = Trim(raw);
	const std::string lower = LowerAscii(value);
	if (value.empty()) return true;
	if ((value.front() == '<' && value.back() == '>') ||
	    lower == "todo" || lower == "tbd" || lower == "unknown" ||
	    lower == "entity" || lower == "component" || lower == "entity_name" ||
	    lower == "component_name" || lower == "your_entity" || lower == "your_component" ||
	    lower == "target entity" || lower == "target component" ||
	    lower == "main entity" || lower == "key component") {
		return true;
	}
	return value.find("主要な Entity") != std::string::npos ||
		value.find("主要なEntity") != std::string::npos ||
		value.find("対象 Entity") != std::string::npos ||
		value.find("対象Entity") != std::string::npos ||
		value.find("対象のEntity") != std::string::npos ||
		value.find("対象 Component") != std::string::npos ||
		value.find("対象Component") != std::string::npos ||
		value.find("対象のComponent") != std::string::npos ||
		value.find("关键") != std::string::npos ||
		lower.find("placeholder") != std::string::npos;
}

bool PayloadRepresentsFailure(const Json& payload) {
	if (!payload.is_object()) return false;
	if (payload.value("failure", false)) return true;
	if (payload.contains("error") && payload.at("error").is_string() &&
	    !Trim(payload.at("error").get<std::string>()).empty()) return true;
	return false;
}

bool PayloadRepresentsUnsatisfied(const Json& payload) {
	if (!payload.is_object()) return false;
	for (const char* key : {"found", "exists", "satisfied"}) {
		if (payload.contains(key) && payload.at(key).is_boolean() &&
		    !payload.at(key).get<bool>()) return true;
	}
	if (payload.contains("status") && payload.at("status").is_string()) {
		const std::string status = LowerAscii(payload.at("status").get<std::string>());
		return status == "not_found" || status == "unsatisfied" || status == "missing";
	}
	return false;
}

void StampRevision(Json* payload) {
	if (payload == nullptr) return;
	if (!payload->is_object()) *payload = Json::object({{"raw", *payload}});
	(*payload)["requestRevision"] = prompts::CurrentRequestRevision();
}

void AddStoredEvidence(AgentContext& ctx, Evidence evidence, std::vector<Evidence>* evidenceOut) {
	const EvidenceId id = ctx.store->AddEvidence(evidence);
	evidence.id = id;
	evidenceOut->push_back(std::move(evidence));
}

Result ResolveDependencyEvidence(AgentContext& ctx, TaskId storeTaskId, const Json& taskSpec, Json* out) {
	*out = Json::object();
	if (!taskSpec.is_object() || !taskSpec.contains("dependencies") ||
	    !taskSpec.at("dependencies").is_array() || taskSpec.at("dependencies").empty()) {
		return Result::Ok();
	}
	if (ctx.store == nullptr) {
		return Result::Fail("RetrievalWorker: TaskStore is unavailable for dependency resolution");
	}

	const std::optional<TaskRow> current = ctx.store->GetTask(storeTaskId);
	if (!current) return Result::Fail("RetrievalWorker: current task is missing from TaskStore");
	const std::vector<TaskRow> siblings = ctx.store->GetChildren(current->parentId);

	for (const Json& depValue : taskSpec.at("dependencies")) {
		if (!depValue.is_string()) return Result::Fail("RetrievalWorker: dependency id is not a string");
		const std::string depId = depValue.get<std::string>();
		const TaskRow* dependency = nullptr;
		for (const TaskRow& candidate : siblings) {
			if (candidate.spec.is_object() && candidate.spec.value("taskId", std::string()) == depId) {
				dependency = &candidate;
				break;
			}
		}
		if (dependency == nullptr) {
			return Result::Fail("RetrievalWorker: dependency task not found: " + depId);
		}
		if (dependency->state != TaskState::Succeeded) {
			return Result::Fail(
				"RetrievalWorker: dependency '" + depId + "' did not satisfy its task (state=" +
				ToString(dependency->state) + ")");
		}

		Json evidenceArray = Json::array();
		for (const Evidence& evidence : ctx.store->GetEvidenceForTask(dependency->id)) {
			evidenceArray.push_back(evidence.ToJson());
		}
		(*out)[depId] = Json::object({
			{"storeTaskId", dependency->id},
			{"state", ToString(dependency->state)},
			{"result", dependency->result},
			{"evidences", std::move(evidenceArray)},
		});
	}
	return Result::Ok();
}

const Json* FindToolDescriptor(const Json& catalog, const std::string& toolName) {
	if (!catalog.is_array()) return nullptr;
	for (const Json& entry : catalog) {
		if (entry.is_object() && entry.value("name", std::string()) == toolName) return &entry;
	}
	return nullptr;
}

bool SchemaHasRequiredFields(const Json& descriptor) {
	if (!descriptor.is_object() || !descriptor.contains("argumentSchema") ||
	    !descriptor.at("argumentSchema").is_object()) return false;
	for (const auto& item : descriptor.at("argumentSchema").items()) {
		if (item.value().is_object() && item.value().value("required", false)) return true;
	}
	return false;
}

void CollectNamedEntities(const Json& value, std::vector<std::string>* names) {
	if (names->size() >= 5) return;
	if (value.is_object()) {
		// found=false等の負の結果は、その内部の検索語を正のBindingとして収集しない。
		if (PayloadRepresentsUnsatisfied(value) || PayloadRepresentsFailure(value)) return;
		if (value.contains("name") && value.at("name").is_string()) {
			const std::string name = Trim(value.at("name").get<std::string>());
			if (!name.empty() && !IsPlaceholderString(name) &&
			    std::find(names->begin(), names->end(), name) == names->end()) {
				names->push_back(name);
			}
		}
		for (const auto& item : value.items()) {
			CollectNamedEntities(item.value(), names);
			if (names->size() >= 5) return;
		}
	} else if (value.is_array()) {
		for (const Json& child : value) {
			CollectNamedEntities(child, names);
			if (names->size() >= 5) return;
		}
	}
}

Json BuildDeterministicCommands(
	const std::unordered_set<std::string>& allowed,
	const Json& filteredCatalog,
	const Json& dependencyEvidence) {

	Json commands = Json::array();
	if (allowed.size() != 1) return commands;
	const std::string toolName = *allowed.begin();
	const Json* descriptor = FindToolDescriptor(filteredCatalog, toolName);
	if (descriptor == nullptr) return commands;

	if (!SchemaHasRequiredFields(*descriptor)) {
		commands.push_back(Json::object({{"tool", toolName}, {"arguments", Json::object()}}));
		return commands;
	}

	if (toolName == "DescribeEntity" || toolName == "FindEntityByName") {
		std::vector<std::string> names;
		CollectNamedEntities(dependencyEvidence, &names);
		const char* field = toolName == "DescribeEntity" ? "entityName" : "name";
		for (const std::string& name : names) {
			commands.push_back(Json::object({
				{"tool", toolName},
				{"arguments", Json::object({{field, name}})},
			}));
		}
	}
	return commands;
}

Json ParseExplicitCommands(const Json& taskSpec) {
	if (!taskSpec.is_object()) return Json::array();
	if (taskSpec.contains("commands") && taskSpec.at("commands").is_array()) {
		return taskSpec.at("commands");
	}
	const std::string description = taskSpec.value("description", std::string());
	const std::string marker = "REPAIR_COMMAND ";
	const std::size_t pos = description.find(marker);
	if (pos == std::string::npos) return Json::array();
	const std::string encoded = Trim(description.substr(pos + marker.size()));
	Json command = Json::parse(encoded, nullptr, false);
	if (!command.is_object() || command.is_discarded()) return Json::array();
	return Json::array({command});
}

Result ValidateGroundedValue(
	const Json& value,
	const std::string& dependencyDump,
	bool requiresDependencyGrounding,
	const std::string& path) {

	if (value.is_string()) {
		const std::string text = Trim(value.get<std::string>());
		if (IsPlaceholderString(text)) {
			return Result::Fail("ungrounded placeholder or blank string at " + path + ": '" + text + "'");
		}
		if (requiresDependencyGrounding && dependencyDump.find(text) == std::string::npos) {
			return Result::Fail("argument at " + path + " is not present in dependency evidence: '" + text + "'");
		}
		return Result::Ok();
	}
	if (value.is_array()) {
		for (std::size_t i = 0; i < value.size(); ++i) {
			Result child = ValidateGroundedValue(
				value[i], dependencyDump, requiresDependencyGrounding,
				path + "[" + std::to_string(i) + "]");
			if (!child) return child;
		}
	}
	if (value.is_object()) {
		for (const auto& item : value.items()) {
			Result child = ValidateGroundedValue(
				item.value(), dependencyDump, requiresDependencyGrounding,
				path + "." + item.key());
			if (!child) return child;
		}
	}
	return Result::Ok();
}

Result ValidateGroundedCommand(
	const Json& command,
	const std::unordered_set<std::string>& allowed,
	const Json& dependencyEvidence) {

	if (!command.is_object() || !command.contains("tool") || !command.at("tool").is_string()) {
		return Result::Fail("command must contain a string tool name");
	}
	const std::string toolName = command.at("tool").get<std::string>();
	if (allowed.count(toolName) == 0) {
		return Result::Fail("tool is not allowed for this task: " + toolName);
	}
	if (!command.contains("arguments") || !command.at("arguments").is_object()) {
		return Result::Fail("command arguments must be an object");
	}

	const bool hasDependencies = dependencyEvidence.is_object() && !dependencyEvidence.empty();
	return ValidateGroundedValue(
		command.at("arguments"), dependencyEvidence.dump(), hasDependencies, "arguments");
}

Evidence MakeFailureEvidence(
	AgentContext& ctx,
	TaskId storeTaskId,
	const std::string& sourceType,
	const std::string& sourceUri,
	const std::string& claim,
	Json payload) {

	Evidence evidence;
	evidence.taskId = storeTaskId;
	evidence.claim = claim;
	if (!payload.is_object()) payload = Json::object({{"raw", std::move(payload)}});
	payload["failure"] = true;
	StampRevision(&payload);
	evidence.payload = std::move(payload);
	evidence.provenance.sourceType = sourceType;
	evidence.provenance.sourceUri = sourceUri;
	evidence.provenance.session = "session_" + std::to_string(ctx.sessionId);
	evidence.confidence = 1.0;
	return evidence;
}

} // namespace

Result RetrievalWorker::Run(
	AgentContext& ctx,
	TaskId storeTaskId,
	const Json& taskSpec,
	std::vector<Evidence>* evidenceOut,
	Json* summaryOut) {
	if (evidenceOut == nullptr) return Result::Fail("RetrievalWorker: evidenceOut is null");
	if (ctx.pipeline == nullptr || ctx.store == nullptr) {
		return Result::Fail("RetrievalWorker: pipeline or store is null");
	}

	Json dependencyEvidence;
	Result dependencyResult = ResolveDependencyEvidence(ctx, storeTaskId, taskSpec, &dependencyEvidence);
	if (!dependencyResult) {
		if (summaryOut != nullptr) {
			*summaryOut = Json::object({
				{"executed", 0}, {"failed", 1}, {"rejected", 0}, {"skipped", true},
				{"outcome", "DependencyUnsatisfied"},
				{"coverageNote", dependencyResult.error},
			});
		}
		return dependencyResult;
	}

	std::unordered_set<std::string> allowed;
	if (taskSpec.is_object() && taskSpec.contains("allowedTools") && taskSpec.at("allowedTools").is_array()) {
		for (const Json& tool : taskSpec.at("allowedTools")) {
			if (tool.is_string()) allowed.insert(tool.get<std::string>());
		}
	}
	if (allowed.empty()) return Result::Fail("RetrievalWorker: task has no allowed tools");

	Json filteredCatalog = Json::array();
	const Json fullCatalog = ctx.pipeline->DescribeTools();
	if (fullCatalog.is_array()) {
		for (const Json& entry : fullCatalog) {
			if (entry.is_object() && entry.contains("name") && entry.at("name").is_string() &&
			    allowed.count(entry.at("name").get<std::string>()) != 0) {
				filteredCatalog.push_back(entry);
			}
		}
	}

	Json commands = ParseExplicitCommands(taskSpec);
	if (commands.empty()) commands = BuildDeterministicCommands(allowed, filteredCatalog, dependencyEvidence);
	if (commands.empty()) {
		Json augmentedTaskSpec = taskSpec;
		augmentedTaskSpec["dependencyEvidence"] = dependencyEvidence;
		const PromptPair prompt = prompts::GenerateQueries(augmentedTaskSpec, filteredCatalog);
		Json raw;
		Result callResult = CallLlmJson(ctx, prompt, &raw);
		if (!callResult) {
			return Result::Fail("RetrievalWorker: query generation failed: " + callResult.error);
		}
		if (raw.is_object() && raw.contains("commands") && raw.at("commands").is_array()) {
			commands = raw.at("commands");
		}
	}

	int attempted = 0;
	int succeeded = 0;
	int failed = 0;
	int rejected = 0;
	int unsatisfied = 0;

	const std::size_t limit = (std::min<std::size_t>)(commands.size(), 5);
	for (std::size_t i = 0; i < limit; ++i) {
		const Json& command = commands[i];
		Result grounded = ValidateGroundedCommand(command, allowed, dependencyEvidence);
		if (!grounded) {
			++rejected;
			const std::string toolName = command.is_object()
				? command.value("tool", std::string("?"))
				: "?";
			AddStoredEvidence(ctx, MakeFailureEvidence(
				ctx, storeTaskId, "CommandValidationError", toolName,
				"Tool command rejected before execution: " + grounded.error,
				Json::object({{"error", grounded.error}, {"command", command}})), evidenceOut);
			break;
		}

		const std::string toolName = command.at("tool").get<std::string>();
		const Json arguments = command.at("arguments");
		CommandRequest request;
		request.taskId = storeTaskId;
		request.issuer = "RetrievalWorker";
		request.tool = toolName;
		request.arguments = arguments;
		request.capability = ctx.token;
		++attempted;

		const CommandResult result = ctx.pipeline->Submit(request);
		if (!result.IsOk()) {
			++failed;
			AddStoredEvidence(ctx, MakeFailureEvidence(
				ctx, storeTaskId, "ToolError", toolName,
				"Tool " + toolName + " failed: " + result.error,
				Json::object({{"status", ToString(result.status)}, {"error", result.error}})), evidenceOut);
			break;
		}
		if (PayloadRepresentsFailure(result.payload)) {
			++failed;
			AddStoredEvidence(ctx, MakeFailureEvidence(
				ctx, storeTaskId, "ToolResultError", toolName,
				"Tool " + toolName + " returned an error result", result.payload), evidenceOut);
			break;
		}
		if (PayloadRepresentsUnsatisfied(result.payload)) {
			++unsatisfied;
			Json payload = result.payload;
			payload["unsatisfied"] = true;
			payload["arguments"] = arguments;
			AddStoredEvidence(ctx, MakeFailureEvidence(
				ctx, storeTaskId, "ToolUnsatisfied", toolName,
				"Tool " + toolName + " completed but did not satisfy the task", payload), evidenceOut);
			break;
		}

		++succeeded;
		Evidence evidence;
		evidence.taskId = storeTaskId;
		evidence.claim = ExtractClaim(result.payload, toolName);
		evidence.payload = result.payload;
		StampRevision(&evidence.payload);
		evidence.provenance.sourceType = "Tool:" + toolName;
		evidence.provenance.sourceUri = toolName;
		evidence.provenance.session = "session_" + std::to_string(ctx.sessionId);
		evidence.confidence = ExtractConfidenceHint(result.payload);
		AddStoredEvidence(ctx, std::move(evidence), evidenceOut);
	}

	if (summaryOut != nullptr) {
		Json summary = Json::object();
		summary["attempted"] = attempted;
		summary["executed"] = succeeded;
		summary["failed"] = failed;
		summary["unsatisfied"] = unsatisfied;
		summary["rejected"] = rejected;
		summary["skipped"] = false;
		summary["requestRevision"] = prompts::CurrentRequestRevision();
		summary["dependencyEvidence"] = dependencyEvidence;
		summary["outcome"] = unsatisfied > 0
			? "Unsatisfied"
			: ((failed > 0 || rejected > 0) ? "Failed" : "Satisfied");
		summary["coverageNote"] = "task '" + ExtractPlanTaskId(taskSpec) + "': succeeded=" +
			std::to_string(succeeded) + " failed=" + std::to_string(failed) +
			" unsatisfied=" + std::to_string(unsatisfied) +
			" rejected=" + std::to_string(rejected);
		*summaryOut = std::move(summary);
	}

	if (commands.empty()) return Result::Fail("RetrievalWorker: no grounded commands were generated");
	if (failed > 0 || rejected > 0) {
		return Result::Fail("RetrievalWorker: one or more commands failed or were rejected");
	}
	if (unsatisfied > 0) {
		return Result::Fail("RetrievalWorker: command completed but task outcome was Unsatisfied");
	}
	if (succeeded == 0) return Result::Fail("RetrievalWorker: no command completed successfully");
	return Result::Ok();
}

} // namespace agentos
