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
#include <unordered_set>
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

		// 実際に撤回されたTask（要求されたが却下されたものは含まない）
		std::vector<TaskId> retiredTasks;
	};

	void Add(Evidence e);

	// requestRevision < 0なら現在のthread-local Request Revisionを使用する。
	void MarkPlannedTask(TaskId taskId, int requestRevision = -1);

	// ---------------------------------
	// Taskの撤回
	// ---------------------------------
	// 「そもそも立てるべきでなかった」と判定されたTaskを計画から外す。
	// 撤回されたTaskは activePlannedTasks から消え、その失敗Evidenceも
	// 集計から除外されるため、coverage / tasksWithoutEvidence /
	// failedEvidenceCount が自然に回復する。
	//
	// これが無かったために、静的なコード質問へ誤って立った実行時トレースTaskの
	// 失敗1件が上記3ゲートを同時に踏み、repairでは二度と解消できない状態
	// （passへ到達する経路が存在しない）に陥っていた。
	// 再計画が「追加」しかできず「撤回」ができないことが原因だった。
	//
	// 安全のため、撤回要求は無条件には通らない。使えるEvidenceを1件でも
	// 産んだTaskは撤回されない（Build内で決定的に判定する）。
	// これによりLLMの誤判断で有用な観測が消えることは原理的に起きない。
	void RequestRetireTask(TaskId taskId);

	BuiltEvidence Build() const;
	static Json ToJson(const BuiltEvidence& built);

private:
	std::vector<Evidence> evidences_;
	std::unordered_map<TaskId, int> plannedTaskRevisions_;
	std::unordered_set<TaskId> retireRequests_;
};

} // namespace agentos
