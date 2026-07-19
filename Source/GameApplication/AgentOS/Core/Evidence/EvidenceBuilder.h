// =======================================================================
//
// EvidenceBuilder.h
//
// 複数Workerが集めたEvidenceをRequest Revision単位で統合する。
// Criticによる要求修正後は旧RevisionのEvidenceを監査履歴としてDBへ残しつつ、
// Reason/CriticのCoverageと失敗判定には最新Revisionだけを使用する。
//
// =======================================================================
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "../AgentOsTypes.h"
#include "../Json.h"
#include "Evidence.h"

namespace agentos {

class EvidenceBuilder {
public:
	struct ContradictionRecord {
		EvidenceId a = kInvalidId;
		EvidenceId b = kInvalidId;
		std::string reason;
	};

	struct BuiltEvidence {
		std::vector<Evidence> evidences;
		std::vector<ContradictionRecord> contradictions;
		double coverage = 1.0;
		std::vector<TaskId> tasksWithoutEvidence;
		std::size_t usableEvidenceCount = 0;
		std::size_t failedEvidenceCount = 0;
		std::size_t supersededEvidenceCount = 0;
		int activeRevision = 0;
	};

	void Add(Evidence e);

	// requestRevision < 0なら現在のthread-local Request Revisionを使用する。
	void MarkPlannedTask(TaskId taskId, int requestRevision = -1);

	BuiltEvidence Build() const;
	static Json ToJson(const BuiltEvidence& built);

private:
	std::vector<Evidence> evidences_;
	std::unordered_map<TaskId, int> plannedTaskRevisions_;
};

} // namespace agentos
