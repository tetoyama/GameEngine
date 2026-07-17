// =======================================================================
//
// PlannerAgent.cpp
//
// =======================================================================
#include "PlannerAgent.h"

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace agentos {

namespace {

constexpr int kMaxTasks = 6;

bool HasCycle(const std::unordered_map<std::string, std::vector<std::string>>& depsMap) {
	enum class Mark { Unvisited, InProgress, Done };
	std::unordered_map<std::string, Mark> marks;
	for (const auto& [id, deps] : depsMap) {
		marks[id] = Mark::Unvisited;
		(void)deps;
	}

	std::function<bool(const std::string&)> visit = [&](const std::string& node) -> bool {
		auto markIt = marks.find(node);
		if (markIt == marks.end()) return false;
		if (markIt->second == Mark::Done) return false;
		if (markIt->second == Mark::InProgress) return true;
		markIt->second = Mark::InProgress;
		auto depsIt = depsMap.find(node);
		if (depsIt != depsMap.end()) {
			for (const std::string& dep : depsIt->second) {
				if (visit(dep)) return true;
			}
		}
		markIt->second = Mark::Done;
		return false;
	};

	for (const auto& [id, deps] : depsMap) {
		(void)deps;
		if (visit(id)) return true;
	}
	return false;
}

bool CatalogHasTool(const Json& toolCatalog, const std::string& name) {
	if (!toolCatalog.is_array()) return false;
	for (const auto& tool : toolCatalog) {
		if (tool.is_object() && tool.value("name", std::string()) == name) return true;
	}
	return false;
}

bool ContainsAny(const std::string& text, std::initializer_list<const char*> needles) {
	for (const char* needle : needles) {
		if (text.find(needle) != std::string::npos) return true;
	}
	return false;
}

std::string CurrentPlanningText(const Json& intake) {
	std::string text;
	if (!intake.is_object()) return text;
	text = intake.value("resolvedRequest", intake.value("goal", std::string()));
	if (intake.contains("symptoms") && intake.at("symptoms").is_array()) {
		text += "\n" + intake.at("symptoms").dump();
	}
	if (intake.contains("constraints") && intake.at("constraints").is_array()) {
		text += "\n" + intake.at("constraints").dump();
	}
	return text;
}

bool TryBuildSceneSnapshotPlan(const Json& intake, const Json& toolCatalog, Json* planOut) {
	if (planOut == nullptr || !intake.is_object()) return false;

	const std::string text = CurrentPlanningText(intake);
	const bool sceneRequest = ContainsAny(text, {"シーン", "Scene", "scene"});
	const bool snapshotRequest = ContainsAny(
		text, {"現在", "状態", "状況", "概要", "報告", "一覧", "全体"});
	const bool temporalRequest = ContainsAny(
		text, {"変化", "推移", "フレーム間", "トレース", "Trace", "trace", "時間経過"});
	const bool narrowedRequest = ContainsAny(text, {
		"だけ", "のみ", "個別", "特定", "対象外", "除外",
		"ではなく", "じゃなく", "そうではなく", "そうじゃなく",
		"詳しく", "絞って", "限定",
		"only", "instead", "exclude", "specific"
	});
	if (!sceneRequest || !snapshotRequest || temporalRequest || narrowedRequest) return false;
	if (!CatalogHasTool(toolCatalog, "ListEntities") ||
	    !CatalogHasTool(toolCatalog, "ListSystems")) {
		return false;
	}

	Json tasks = Json::array();
	tasks.push_back(Json::object({
		{"taskId", "T1"},
		{"type", "RuntimeObservation"},
		{"description", "アクティブSceneのEntity一覧を取得する"},
		{"dependencies", Json::array()},
		{"allowedTools", Json::array({"ListEntities"})},
		{"searchHints", Json::array()},
	}));
	tasks.push_back(Json::object({
		{"taskId", "T2"},
		{"type", "RuntimeObservation"},
		{"description", "登録済みSystemTaskと依存関係を取得する"},
		{"dependencies", Json::array()},
		{"allowedTools", Json::array({"ListSystems"})},
		{"searchHints", Json::array()},
	}));

	Json analysisDependencies = Json::array({"T1", "T2"});
	if (CatalogHasTool(toolCatalog, "DescribeEntity")) {
		tasks.push_back(Json::object({
			{"taskId", "T3"},
			{"type", "RuntimeObservation"},
			{"description", "T1で取得した名前付きEntityから代表的な最大5件のComponent詳細を取得する"},
			{"dependencies", Json::array({"T1"})},
			{"allowedTools", Json::array({"DescribeEntity"})},
			{"searchHints", Json::array()},
		}));
		analysisDependencies.push_back("T3");
	}

	tasks.push_back(Json::object({
		{"taskId", "T4"},
		{"type", "Analysis"},
		{"description", "取得済みのScene観測結果をユーザー要求に沿って統合する"},
		{"dependencies", analysisDependencies},
		{"allowedTools", Json::array()},
		{"searchHints", Json::array()},
	}));

	*planOut = Json::object({
		{"tasks", std::move(tasks)},
		{"route", "deterministic_scene_snapshot"},
	});
	return true;
}

bool ValidatePlan(const Json& plan, const Json& toolCatalog, std::string* error) {
	if (!plan.is_object() || !plan.contains("tasks") || !plan.at("tasks").is_array()) {
		*error = "plan must contain a 'tasks' array";
		return false;
	}
	const Json& tasks = plan.at("tasks");
	if (tasks.empty()) {
		*error = "tasks array must not be empty";
		return false;
	}
	if (tasks.size() > static_cast<std::size_t>(kMaxTasks)) {
		*error = "tasks array exceeds max of " + std::to_string(kMaxTasks);
		return false;
	}

	std::unordered_set<std::string> toolNames;
	if (toolCatalog.is_array()) {
		for (const auto& t : toolCatalog) {
			if (t.is_object() && t.contains("name") && t.at("name").is_string()) {
				toolNames.insert(t.at("name").get<std::string>());
			}
		}
	}

	static const std::unordered_set<std::string> kValidTypes = {
		"RuntimeObservation", "CodeSearch", "Trace", "Analysis"};

	std::unordered_set<std::string> seenIds;
	std::unordered_map<std::string, std::vector<std::string>> depsMap;

	for (const auto& task : tasks) {
		if (!task.is_object() || !task.contains("taskId") || !task.at("taskId").is_string()) {
			*error = "each task must have a string taskId";
			return false;
		}
		const std::string id = task.at("taskId").get<std::string>();
		if (id.empty()) {
			*error = "taskId must not be empty";
			return false;
		}
		if (seenIds.count(id) != 0) {
			*error = "duplicate taskId: " + id;
			return false;
		}
		seenIds.insert(id);

		if (!task.contains("type") || !task.at("type").is_string() ||
		    kValidTypes.count(task.at("type").get<std::string>()) == 0) {
			*error = "task '" + id + "' has invalid or missing type";
			return false;
		}

		std::vector<std::string> deps;
		if (task.contains("dependencies") && task.at("dependencies").is_array()) {
			for (const auto& d : task.at("dependencies")) {
				if (!d.is_string()) {
					*error = "task '" + id + "' has a non-string dependency";
					return false;
				}
				deps.push_back(d.get<std::string>());
			}
		}
		depsMap[id] = deps;

		if (task.contains("allowedTools") && task.at("allowedTools").is_array()) {
			for (const auto& tool : task.at("allowedTools")) {
				if (!tool.is_string() || toolNames.count(tool.get<std::string>()) == 0) {
					*error = "task '" + id + "' allowedTools references a tool not in the catalog";
					return false;
				}
			}
		}
	}

	for (const auto& [id, deps] : depsMap) {
		for (const auto& d : deps) {
			if (seenIds.count(d) == 0) {
				*error = "task '" + id + "' depends on unknown taskId: " + d;
				return false;
			}
		}
	}

	if (HasCycle(depsMap)) {
		*error = "dependency cycle detected among tasks";
		return false;
	}
	return true;
}

void NormalizePlan(Json* plan) {
	if (plan == nullptr || !plan->is_object() || !plan->contains("tasks")) return;
	Json& tasks = (*plan)["tasks"];
	if (!tasks.is_array()) return;
	for (Json& task : tasks) {
		if (!task.is_object()) continue;
		if (task.contains("taskId") && task["taskId"].is_number_integer()) {
			task["taskId"] = std::to_string(task["taskId"].get<std::int64_t>());
		}
		if (task.contains("dependencies") && task["dependencies"].is_array()) {
			for (Json& dep : task["dependencies"]) {
				if (dep.is_number_integer()) dep = std::to_string(dep.get<std::int64_t>());
			}
		}
		const bool hasTools = task.contains("allowedTools") &&
			task["allowedTools"].is_array() && !task["allowedTools"].empty();
		if (task.value("type", std::string()) == "Analysis" && hasTools) {
			task["type"] = "RuntimeObservation";
		}
	}
}

} // namespace

Result PlannerAgent::Run(AgentContext& ctx, const Json& intake, const Json& toolCatalog, Json* planOut) {
	if (planOut == nullptr) return Result::Fail("PlannerAgent: planOut is null");

	if (TryBuildSceneSnapshotPlan(intake, toolCatalog, planOut)) return Result::Ok();

	const PromptPair prompt = prompts::Plan(intake, toolCatalog, kMaxTasks);
	Json raw;
	Result callResult = CallLlmJson(ctx, prompt, &raw);
	if (!callResult) return callResult;

	NormalizePlan(&raw);
	std::string validationError;
	if (ValidatePlan(raw, toolCatalog, &validationError)) {
		*planOut = std::move(raw);
		return Result::Ok();
	}

	PromptPair retryPrompt = prompt;
	retryPrompt.user +=
		"\n\n前回の出力は次の理由で拒否されました。修正して再出力してください: " +
		validationError;

	Json retryRaw;
	Result retryCallResult = CallLlmJson(ctx, retryPrompt, &retryRaw);
	if (!retryCallResult) return retryCallResult;

	NormalizePlan(&retryRaw);
	std::string retryValidationError;
	if (ValidatePlan(retryRaw, toolCatalog, &retryValidationError)) {
		*planOut = std::move(retryRaw);
		return Result::Ok();
	}

	return Result::Fail("PlannerAgent: plan validation failed twice: " + retryValidationError);
}

} // namespace agentos
