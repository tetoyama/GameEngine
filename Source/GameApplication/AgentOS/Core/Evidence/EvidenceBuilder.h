// =======================================================================
//
// EvidenceBuilder.h
//
// 複数Workerが集めたEvidence列を、決定的（LLM不使用）に統合する（構想§5.4）。
// - 重複除去: claim + payload dump が完全一致するものは先勝ち。
// - 矛盾検出: 下記2ルールのみ（決定的・網羅ではなく実用ヒューリスティック）。
//   (1) payloadに文字列キー"target"と任意キー"value"を両方持つEvidence同士で、
//       target一致・provenance.frame一致（>=0）・valueのdumpが不一致なら矛盾。
//   (2) claimが完全一致するEvidence同士で、payload["value"]のdumpが不一致なら矛盾。
// - Coverage: MarkPlannedTaskで登録したTaskのうち、1件以上Evidenceを産んだ割合。
//   計画Taskが1件も無ければ1.0とする。
//
// =======================================================================
#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "../AgentOsTypes.h"
#include "../Json.h"
#include "Evidence.h"

namespace agentos {

class EvidenceBuilder {
public:
	// 矛盾ペアの記録。a/bの順序はAdd()された順（先に追加された方がa）。
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
	};

	void Add(Evidence e);

	// 計画された検索Task（RetrievalWorker等）を登録する。Coverage算出に使う。
	void MarkPlannedTask(TaskId taskId);

	BuiltEvidence Build() const;

	static Json ToJson(const BuiltEvidence& built);

private:
	std::vector<Evidence> evidences_;
	std::vector<TaskId> plannedTasks_;
};

} // namespace agentos
