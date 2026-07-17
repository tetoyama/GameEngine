// =======================================================================
//
// PlannerAgent.cpp
//
// =======================================================================
#include "PlannerAgent.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace agentos {

namespace {

constexpr int kMaxTasks = 6;

// depsMap（taskId → dependencies）に循環があるかをDFSで検査する。
bool HasCycle(const std::unordered_map<std::string, std::vector<std::string>>& depsMap) {
	enum class Mark { Unvisited, InProgress, Done };
	std::unordered_map<std::string, Mark> marks;
	for (const auto& [id, deps] : depsMap) {
		marks[id] = Mark::Unvisited;
		(void)deps;
	}

	std::function<bool(const std::string&)> visit = [&](const std::string& node) -> bool {
		auto markIt = marks.find(node);
		if (markIt == marks.end()) {
			return false; // 未知ノード（存在チェックは別途行う）
		}
		if (markIt->second == Mark::Done) {
			return false;
		}
		if (markIt->second == Mark::InProgress) {
			return true; // 循環検出
		}
		markIt->second = Mark::InProgress;
		auto depsIt = depsMap.find(node);
		if (depsIt != depsMap.end()) {
			for (const std::string& dep : depsIt->second) {
				if (visit(dep)) {
					return true;
				}
			}
		}
		markIt->second = Mark::Done;
		return false;
	};

	for (const auto& [id, deps] : depsMap) {
		(void)deps;
		if (visit(id)) {
			return true;
		}
	}
	return false;
}

// PlanのJSONを決定的に検証する。合格すればtrue、不合格ならerrorを埋めてfalse。
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

	// 依存先が実在するtaskIdであることを確認する。
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

// LLMがtaskId/dependenciesを整数で返してきた場合に文字列へ正規化する。
//
// [実機対応] プロンプトのスキーマ例と検証の型要求が食い違うと、ローカルモデルは
// 「スキーマはintegerなのに拒否理由はstringと言っている」という矛盾で混乱し、
// リトライを浪費する事例が実LLMログで確認された。プロンプト側もstringへ統一したが、
// integerで返すモデルも受理できるようにここで吸収する（LLM出力は信頼しない原則の一環）。
void NormalizeTaskIds(Json* plan) {
	if (plan == nullptr || !plan->is_object() || !plan->contains("tasks")) {
		return;
	}
	Json& tasks = (*plan)["tasks"];
	if (!tasks.is_array()) {
		return;
	}
	for (Json& task : tasks) {
		if (!task.is_object()) {
			continue;
		}
		if (task.contains("taskId") && task["taskId"].is_number_integer()) {
			task["taskId"] = std::to_string(task["taskId"].get<std::int64_t>());
		}
		if (task.contains("dependencies") && task["dependencies"].is_array()) {
			for (Json& dep : task["dependencies"]) {
				if (dep.is_number_integer()) {
					dep = std::to_string(dep.get<std::int64_t>());
				}
			}
		}
	}
}

} // namespace

Result PlannerAgent::Run(AgentContext& ctx, const Json& intake, const Json& toolCatalog, Json* planOut) {
	if (planOut == nullptr) {
		return Result::Fail("PlannerAgent: planOut is null");
	}

	const PromptPair prompt = prompts::Plan(intake, toolCatalog, kMaxTasks);

	Json raw;
	Result callResult = CallLlmJson(ctx, prompt, &raw);
	if (!callResult) {
		return callResult;
	}

	NormalizeTaskIds(&raw);

	std::string validationError;
	if (ValidatePlan(raw, toolCatalog, &validationError)) {
		*planOut = std::move(raw);
		return Result::Ok();
	}

	// --- バリデーション失敗 → エラーを添えて1回だけリトライ ---
	PromptPair retryPrompt = prompt;
	retryPrompt.user += "\n\n前回の出力は次の理由で拒否されました。修正して再出力してください: " + validationError;

	Json retryRaw;
	Result retryCallResult = CallLlmJson(ctx, retryPrompt, &retryRaw);
	if (!retryCallResult) {
		return retryCallResult;
	}

	NormalizeTaskIds(&retryRaw);

	std::string retryValidationError;
	if (ValidatePlan(retryRaw, toolCatalog, &retryValidationError)) {
		*planOut = std::move(retryRaw);
		return Result::Ok();
	}

	return Result::Fail("PlannerAgent: plan validation failed twice: " + retryValidationError);
}

} // namespace agentos
