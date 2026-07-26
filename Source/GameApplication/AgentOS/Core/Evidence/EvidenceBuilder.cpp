// =======================================================================
//
// EvidenceBuilder.cpp
//
// =======================================================================
#include "EvidenceBuilder.h"

#include <algorithm>
#include <unordered_set>

#include "../Llm/PromptTemplates.h"

namespace agentos {

void EvidenceBuilder::Add(Evidence e) {
	evidences_.push_back(std::move(e));
}

void EvidenceBuilder::MarkPlannedTask(TaskId taskId, int requestRevision) {
	const int revision = requestRevision >= 0
		? requestRevision
		: prompts::CurrentRequestRevision();
	plannedTaskRevisions_[taskId] = revision;
}

void EvidenceBuilder::RequestRetireTask(TaskId taskId) {
	// ここでは要求を記録するだけ。実際に撤回してよいかはBuild内で判定する。
	retireRequests_.insert(taskId);
}

namespace {

std::string DedupKey(const Evidence& e) {
	return e.claim + "\x1f" + e.payload.dump();
}

int EvidenceRevision(const Evidence& evidence) {
	if (evidence.payload.is_object()) {
		return evidence.payload.value("requestRevision", 0);
	}
	return 0;
}

bool ExtractTargetValue(
	const Evidence& e,
	std::string* targetOut,
	std::string* valueDumpOut) {
	if (!e.payload.is_object() ||
	    !e.payload.contains("target") || !e.payload.at("target").is_string() ||
	    !e.payload.contains("value")) {
		return false;
	}
	*targetOut = e.payload.at("target").get<std::string>();
	*valueDumpOut = e.payload.at("value").dump();
	return true;
}

bool IsFailureEvidence(const Evidence& evidence) {
	const std::string& sourceType = evidence.provenance.sourceType;
	if (sourceType == "ToolError" || sourceType == "ToolResultError" ||
	    sourceType == "ToolUnsatisfied" || sourceType == "CommandValidationError" ||
	    sourceType == "DependencyUnsatisfied") {
		return true;
	}
	if (!evidence.payload.is_object()) return false;
	if (evidence.payload.value("failure", false) || evidence.payload.value("unsatisfied", false)) {
		return true;
	}
	return evidence.payload.contains("error") && evidence.payload.at("error").is_string() &&
		!evidence.payload.at("error").get<std::string>().empty();
}

} // namespace

EvidenceBuilder::BuiltEvidence EvidenceBuilder::Build() const {
	BuiltEvidence built;

	for (const auto& [taskId, revision] : plannedTaskRevisions_) {
		(void)taskId;
		built.activeRevision = (std::max)(built.activeRevision, revision);
	}
	for (const Evidence& evidence : evidences_) {
		built.activeRevision = (std::max)(built.activeRevision, EvidenceRevision(evidence));
	}

	// --- 撤回してよいTaskを決める（決定的な安全ガード） ---
	// 使えるEvidenceを1件でも産んだTaskは撤回しない。
	// 撤回の判断はLLMが行うため、誤判断で有用な観測が消えないよう
	// ここでプログラム側が拒否できるようにしておく。
	std::unordered_set<TaskId> retired;
	for (const TaskId taskId : retireRequests_) {
		bool producedUsableEvidence = false;
		for (const Evidence& evidence : evidences_) {
			if (evidence.taskId != taskId) continue;
			if (!IsFailureEvidence(evidence)) {
				producedUsableEvidence = true;
				break;
			}
		}
		if (!producedUsableEvidence) {
			retired.insert(taskId);
			built.retiredTasks.push_back(taskId);
		}
	}
	std::sort(built.retiredTasks.begin(), built.retiredTasks.end());

	std::unordered_set<std::string> seenKeys;
	for (const Evidence& evidence : evidences_) {
		// 撤回されたTaskのEvidence（失敗のみ）は集計から外す。
		// これによりfailedEvidenceCountが回復する。
		if (retired.count(evidence.taskId) != 0) continue;

		if (EvidenceRevision(evidence) != built.activeRevision) {
			++built.supersededEvidenceCount;
			continue;
		}
		const std::string key = DedupKey(evidence);
		if (seenKeys.count(key) != 0) continue;
		seenKeys.insert(key);
		built.evidences.push_back(evidence);
	}

	for (const Evidence& evidence : built.evidences) {
		if (IsFailureEvidence(evidence)) ++built.failedEvidenceCount;
		else ++built.usableEvidenceCount;
	}

	const std::size_t n = built.evidences.size();
	for (std::size_t i = 0; i < n; ++i) {
		for (std::size_t j = i + 1; j < n; ++j) {
			const Evidence& a = built.evidences[i];
			const Evidence& b = built.evidences[j];

			std::string targetA, valueA, targetB, valueB;
			const bool hasTvA = ExtractTargetValue(a, &targetA, &valueA);
			const bool hasTvB = ExtractTargetValue(b, &targetB, &valueB);
			if (hasTvA && hasTvB && targetA == targetB &&
			    a.provenance.frame >= 0 && a.provenance.frame == b.provenance.frame &&
			    valueA != valueB) {
				built.contradictions.push_back(ContradictionRecord{
					a.id, b.id,
					"target='" + targetA + "' frame=" + std::to_string(a.provenance.frame) +
						" で値が矛盾: " + valueA + " vs " + valueB});
			}

			if (!a.claim.empty() && a.claim == b.claim &&
			    a.payload.is_object() && b.payload.is_object() &&
			    a.payload.contains("value") && b.payload.contains("value")) {
				const std::string va = a.payload.at("value").dump();
				const std::string vb = b.payload.at("value").dump();
				if (va != vb) {
					built.contradictions.push_back(ContradictionRecord{
						a.id, b.id,
						"claim='" + a.claim + "' で value が矛盾: " + va + " vs " + vb});
				}
			}
		}
	}

	std::vector<TaskId> activePlannedTasks;
	for (const auto& [taskId, revision] : plannedTaskRevisions_) {
		// 撤回されたTaskはcoverageの分母から外す。
		// これによりcoverageが1.0へ戻れるようになり、
		// tasksWithoutEvidenceからも消える。
		if (retired.count(taskId) != 0) continue;
		if (revision == built.activeRevision) activePlannedTasks.push_back(taskId);
	}

	if (activePlannedTasks.empty()) {
		built.coverage = 1.0;
	} else {
		std::unordered_set<TaskId> tasksWithUsableEvidence;
		for (const Evidence& evidence : built.evidences) {
			if (!IsFailureEvidence(evidence)) tasksWithUsableEvidence.insert(evidence.taskId);
		}
		std::size_t covered = 0;
		for (const TaskId task : activePlannedTasks) {
			if (tasksWithUsableEvidence.count(task) != 0) ++covered;
			else built.tasksWithoutEvidence.push_back(task);
		}
		built.coverage = static_cast<double>(covered) /
			static_cast<double>(activePlannedTasks.size());
	}

	return built;
}

Json EvidenceBuilder::ToJson(const BuiltEvidence& built) {
	Json j = Json::object();

	Json evidences = Json::array();
	for (const Evidence& e : built.evidences) evidences.push_back(e.ToJson());
	j["evidences"] = std::move(evidences);

	Json contradictions = Json::array();
	for (const ContradictionRecord& c : built.contradictions) {
		contradictions.push_back(Json::object({
			{"a", c.a}, {"b", c.b}, {"reason", c.reason},
		}));
	}
	j["contradictions"] = std::move(contradictions);
	j["coverage"] = built.coverage;
	j["tasksWithoutEvidence"] = built.tasksWithoutEvidence;
	j["usableEvidenceCount"] = built.usableEvidenceCount;
	j["failedEvidenceCount"] = built.failedEvidenceCount;
	j["supersededEvidenceCount"] = built.supersededEvidenceCount;
	j["activeRevision"] = built.activeRevision;
	// 撤回は監査対象。何が計画から外れたかがtranscriptに残るようにする。
	j["retiredTasks"] = built.retiredTasks;

	const Json requestContext = prompts::CurrentConversationRequestContext();
	if (requestContext.is_object() && !requestContext.empty()) j["requestContext"] = requestContext;
	return j;
}

} // namespace agentos
