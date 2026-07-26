// =======================================================================
//
// CodeInvestigationPlanner.h
//
// 静的コード調査をRuntime Entity観測へ誤ルーティングしないための決定的Plan。
// PlannerAgent.cppからのみ利用する内部ヘルパとしてheader-onlyで置く。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <initializer_list>
#include <regex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "PlannerAgent.h"

namespace agentos::planner_internal {

namespace detail {

inline bool CatalogHasTool(const Json& catalog, const std::string& name) {
	if(!catalog.is_array()) return false;
	for(const Json& tool : catalog) {
		if(tool.is_object() && tool.value("name", std::string()) == name) return true;
	}
	return false;
}

inline bool ContainsAny(const std::string& text, std::initializer_list<const char*> needles) {
	for(const char* needle : needles) {
		if(needle != nullptr && text.find(needle) != std::string::npos) return true;
	}
	return false;
}

inline void AppendUnique(std::vector<std::string>* values, const std::string& value) {
	if(values == nullptr || value.empty()) return;
	if(std::find(values->begin(), values->end(), value) == values->end()) {
		values->push_back(value);
	}
}

inline std::string PlanningText(const Json& intake) {
	if(!intake.is_object()) return {};
	return intake.value("resolvedRequest", intake.value("goal", std::string()));
}

inline std::vector<std::string> QualifiedSymbols(const std::string& text) {
	std::vector<std::string> values;
	static const std::regex re(R"([A-Za-z_][A-Za-z0-9_]*(::[A-Za-z_][A-Za-z0-9_]*)+)");
	for(auto it = std::sregex_iterator(text.begin(), text.end(), re);
	    it != std::sregex_iterator(); ++it) {
		AppendUnique(&values, it->str());
	}
	return values;
}

inline std::vector<std::string> Identifiers(const std::string& text) {
	std::vector<std::string> values;
	static const std::regex re(R"([A-Za-z_][A-Za-z0-9_]{3,})");
	static const std::unordered_set<std::string> stop = {
		"CodeSearch", "GetSymbolInfo", "FindCodeReferences", "RuntimeObservation",
		"Trace", "Analysis", "Tool", "Evidence", "Scene", "Entity", "Component",
		"class", "struct", "function", "implementation", "singleton",
	};
	for(auto it = std::sregex_iterator(text.begin(), text.end(), re);
	    it != std::sregex_iterator(); ++it) {
		const std::string value = it->str();
		if(stop.count(value) == 0) AppendUnique(&values, value);
	}
	return values;
}

inline bool LooksLikeCodeRequest(const std::string& text) {
	if(!QualifiedSymbols(text).empty()) return true;
	static const std::regex sourceFileRe(
		R"([A-Za-z0-9_./\\-]+\.(c|cc|cpp|cxx|h|hh|hpp|hxx))",
		std::regex::icase);
	if(std::regex_search(text, sourceFileRe)) return true;
	return ContainsAny(text, {
		"コード", "ソース", "実装", "定義", "関数", "メソッド", "クラス",
		"呼び出", "参照元", "使用箇所", "処理経路", "実行経路", "依存関係",
		"postcondition", "Postcondition", "singleton", "シングルトン",
		"GetInstance", "CheckPostcondition", "CommandPipeline", "PlannerAgent",
	});
}

inline bool CallerRequest(const std::string& text) {
	return ContainsAny(text, {
		"呼び出している", "呼び出し元", "呼び出し箇所", "参照元", "使用箇所",
		"caller", "call site", "callsite",
	});
}

inline bool ExactImplementationRequest(const std::string& text) {
	return ContainsAny(text, {
		"実装を見せ", "実装を表示", "実装コード", "全文", "そのまま表示",
		"implementation", "definition",
	});
}

inline bool AbsenceCheck(const std::string& text) {
	return ContainsAny(text, {
		"実装されている", "存在する", "存在しない", "ある？", "ありますか",
		"呼ばれている", "使われている", "postcondition", "Postcondition",
	});
}

inline std::vector<std::string> SearchQueries(const std::string& text) {
	std::vector<std::string> queries = Identifiers(text);
	if(ContainsAny(text, {"postcondition", "Postcondition"})) {
		AppendUnique(&queries, "CheckPostcondition");
		AppendUnique(&queries, "PostconditionFailed");
		AppendUnique(&queries, "ICommandExecutor");
		AppendUnique(&queries, "CommandPipeline::Submit");
	}
	if(ContainsAny(text, {"パネル", "Panel", "UI"}) &&
	   ContainsAny(text, {"閉じ", "非表示", "ツール実行", "timeout", "失敗"})) {
		AppendUnique(&queries, "AgentOSPanel::Draw");
		AppendUnique(&queries, "PumpMainThread");
		AppendUnique(&queries, "MainThreadDispatcher");
	}
	if(ContainsAny(text, {"シングルトン", "singleton", "GetInstance"})) {
		AppendUnique(&queries, "AgentOSService");
		AppendUnique(&queries, "GetInstance");
		AppendUnique(&queries, "EngineContext::Register");
		AppendUnique(&queries, "EngineContext::Get");
	}
	if(ContainsAny(text, {"処理経路", "実行経路", "パイプライン"})) {
		AppendUnique(&queries, "Orchestrator::RunSession");
		AppendUnique(&queries, "RetrievalWorker::Run");
		AppendUnique(&queries, "CommandPipeline::Submit");
	}
	return queries;
}

} // namespace detail

inline bool TryBuildCodeInvestigationPlan(
	const Json& intake,
	const Json& toolCatalog,
	Json* planOut) {

	// Plannerが受け取った正規のCatalogを、後続のCritic/Repairへ引き継ぐ。
	// このヘルパはPlannerAgent::Runの最初に呼ばれるため、静的コード調査以外の
	// LLM Planner経路でも同一CatalogがセッションContextへ保存される。
	prompts::SetCurrentToolCatalog(toolCatalog);

	if(planOut == nullptr || !intake.is_object()) return false;
	const std::string text = detail::PlanningText(intake);
	if(!detail::LooksLikeCodeRequest(text)) return false;

	const bool hasCodeSearch = detail::CatalogHasTool(toolCatalog, "CodeSearch");
	const bool hasSymbolInfo = detail::CatalogHasTool(toolCatalog, "GetSymbolInfo");
	const bool hasReferences = detail::CatalogHasTool(toolCatalog, "FindCodeReferences");
	if(!hasCodeSearch && !hasSymbolInfo && !hasReferences) return false;

	constexpr int kMaxTasks = 6;
	Json tasks = Json::array();
	Json dependencies = Json::array();
	int nextTask = 1;

	auto addTask = [&](const std::string& tool, Json arguments,
	                   const std::string& description, const std::string& hint) {
		if(nextTask >= kMaxTasks || !detail::CatalogHasTool(toolCatalog, tool)) return;
		const std::string id = "T" + std::to_string(nextTask++);
		tasks.push_back(Json::object({
			{"taskId", id},
			{"type", "CodeSearch"},
			{"description", description},
			{"dependencies", Json::array()},
			{"allowedTools", Json::array({tool})},
			{"searchHints", Json::array({hint})},
			{"commands", Json::array({Json::object({
				{"tool", tool},
				{"arguments", std::move(arguments)},
			})})},
		}));
		dependencies.push_back(id);
	};

	const std::vector<std::string> symbols = detail::QualifiedSymbols(text);
	const bool caller = detail::CallerRequest(text);
	const bool exactImplementation = detail::ExactImplementationRequest(text);

	if(hasSymbolInfo && !symbols.empty()) {
		addTask(
			"GetSymbolInfo",
			Json::object({{"name", symbols.front()}}),
			"正確なシンボル定義を取得する: " + symbols.front(),
			symbols.front());
	}

	if(caller && hasReferences && !symbols.empty()) {
		const std::string& symbol = symbols.front();
		const std::size_t separator = symbol.rfind("::");
		const std::string leaf = separator == std::string::npos
			? symbol
			: symbol.substr(separator + 2);
		addTask(
			"FindCodeReferences",
			Json::object({{"query", leaf + "("}, {"topK", 100}}),
			"ソース全体から呼び出し候補を完全走査する: " + leaf + "(",
			leaf + "(");
	}

	if(!(exactImplementation && !symbols.empty()) && !caller) {
		for(const std::string& query : detail::SearchQueries(text)) {
			if(nextTask >= kMaxTasks) break;
			if(!symbols.empty() && query == symbols.front()) continue;

			if(detail::AbsenceCheck(text) && hasReferences &&
			   query.find(' ') == std::string::npos) {
				addTask(
					"FindCodeReferences",
					Json::object({{"query", query}, {"topK", 100}}),
					"ソース全体で識別子の出現有無を完全走査する: " + query,
					query);
			} else if(hasCodeSearch) {
				addTask(
					"CodeSearch",
					Json::object({{"query", query}, {"topK", 10}}),
					"ソースコード索引を検索する: " + query,
					query);
			}
		}
	}

	if(caller && hasReferences && symbols.empty()) {
		const std::vector<std::string> identifiers = detail::Identifiers(text);
		if(!identifiers.empty()) {
			addTask(
				"FindCodeReferences",
				Json::object({{"query", identifiers.front() + "("}, {"topK", 100}}),
				"ソース全体から呼び出し候補を完全走査する: " + identifiers.front() + "(",
				identifiers.front() + "(");
		}
	}

	if(tasks.empty()) return false;

	tasks.push_back(Json::object({
		{"taskId", "T" + std::to_string(nextTask)},
		{"type", "Analysis"},
		{"description", "コードEvidenceを統合し、定義・参照箇所・完全走査による不在確認・未確認点を区別する"},
		{"dependencies", dependencies},
		{"allowedTools", Json::array()},
		{"searchHints", Json::array()},
	}));

	*planOut = Json::object({
		{"tasks", std::move(tasks)},
		{"route", "deterministic_code_investigation"},
	});
	return true;
}

} // namespace agentos::planner_internal
