// =======================================================================
//
// EvidenceBuilder.cpp
//
// =======================================================================
#include "EvidenceBuilder.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace agentos {

void EvidenceBuilder::Add(Evidence e) {
	evidences_.push_back(std::move(e));
}

void EvidenceBuilder::MarkPlannedTask(TaskId taskId) {
	plannedTasks_.push_back(taskId);
}

namespace {

// claim + payload dumpの完全一致を重複キーとする。
std::string DedupKey(const Evidence& e) {
	return e.claim + "\x1f" + e.payload.dump();
}

// payloadから "target"（文字列）と "value" を取り出す。
// どちらか欠けている、またはtargetが文字列でない場合はfalseを返す。
bool ExtractTargetValue(const Evidence& e, std::string* targetOut, std::string* valueDumpOut) {
	if (!e.payload.is_object()) {
		return false;
	}
	if (!e.payload.contains("target") || !e.payload.at("target").is_string()) {
		return false;
	}
	if (!e.payload.contains("value")) {
		return false;
	}
	*targetOut = e.payload.at("target").get<std::string>();
	*valueDumpOut = e.payload.at("value").dump();
	return true;
}

} // namespace

EvidenceBuilder::BuiltEvidence EvidenceBuilder::Build() const {
	BuiltEvidence built;

	// --- 重複除去（先勝ち） ---
	std::unordered_set<std::string> seenKeys;
	built.evidences.reserve(evidences_.size());
	for (const Evidence& e : evidences_) {
		const std::string key = DedupKey(e);
		if (seenKeys.count(key) != 0) {
			continue; // 既出。先に追加された方を残す。
		}
		seenKeys.insert(key);
		built.evidences.push_back(e);
	}

	// --- 矛盾検出 ---
	// ルール1: 同一target（文字列）・同一frame（>=0）でvalueが食い違う。
	// ルール2: claim文字列が完全一致し、payload["value"]が食い違う。
	const std::size_t n = built.evidences.size();
	for (std::size_t i = 0; i < n; ++i) {
		for (std::size_t j = i + 1; j < n; ++j) {
			const Evidence& a = built.evidences[i];
			const Evidence& b = built.evidences[j];

			std::string targetA, valueA, targetB, valueB;
			const bool hasTvA = ExtractTargetValue(a, &targetA, &valueA);
			const bool hasTvB = ExtractTargetValue(b, &targetB, &valueB);
			if (hasTvA && hasTvB &&
			    targetA == targetB &&
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

	// --- Coverage ---
	if (plannedTasks_.empty()) {
		built.coverage = 1.0;
	} else {
		std::unordered_set<TaskId> tasksWithEvidence;
		for (const Evidence& e : built.evidences) {
			tasksWithEvidence.insert(e.taskId);
		}
		std::size_t covered = 0;
		for (const TaskId task : plannedTasks_) {
			if (tasksWithEvidence.count(task) != 0) {
				++covered;
			} else {
				built.tasksWithoutEvidence.push_back(task);
			}
		}
		built.coverage = static_cast<double>(covered) / static_cast<double>(plannedTasks_.size());
	}

	return built;
}

Json EvidenceBuilder::ToJson(const BuiltEvidence& built) {
	Json j = Json::object();

	Json evidences = Json::array();
	for (const Evidence& e : built.evidences) {
		evidences.push_back(e.ToJson());
	}
	j["evidences"] = std::move(evidences);

	Json contradictions = Json::array();
	for (const ContradictionRecord& c : built.contradictions) {
		contradictions.push_back(Json::object({
			{"a", c.a},
			{"b", c.b},
			{"reason", c.reason},
		}));
	}
	j["contradictions"] = std::move(contradictions);

	j["coverage"] = built.coverage;
	j["tasksWithoutEvidence"] = built.tasksWithoutEvidence;

	return j;
}

} // namespace agentos
