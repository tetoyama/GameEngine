// =======================================================================
//
// EarlyStopping.h
//
// 調査ループの打ち切り判断（構想§7.2 / 00_Architecture.md §10）。
// 単一Confidenceでは判断せず、直近ラウンドの新規Evidence量・仮説順位の
// 変化・未解決矛盾数・同一失敗の反復回数・残Budget比を組み合わせて決める。
//
// =======================================================================
#pragma once

#include <string>
#include <vector>

#include "../AgentOsTypes.h"
#include "../Budget/Budget.h"

namespace agentos {

class EarlyStopping {
public:
	struct StopDecision {
		bool stop = false;
		std::string reason;
	};

	// 1ラウンド分の結果を記録する。
	// newEvidenceCount: このラウンドで新たに追加されたEvidence数。
	// topHypothesis: このラウンド終了時点の最有力仮説ID（無ければkInvalidId）。
	// unresolvedContradictions: 未解決の矛盾件数。
	// sameFailureRepeated: 直前ラウンドと同一の失敗が繰り返されたか。
	void RecordRound(int newEvidenceCount, LogicNodeId topHypothesis,
	                  int unresolvedContradictions, bool sameFailureRepeated);

	// 打ち切り判定。ルール（優先順）:
	//   1. budget.RemainingRatio() < 0.2 なら停止。
	//   2. 同一失敗が3回以上連続したら停止。
	//   3. 直近2ラウンドの新規Evidenceが共に0、かつ最有力仮説が変化していなければ停止。
	//   4. それ以外は継続。
	StopDecision Evaluate(const BudgetTracker& budget) const;

private:
	struct RoundRecord {
		int newEvidenceCount = 0;
		LogicNodeId topHypothesis = kInvalidId;
		int unresolvedContradictions = 0;
		bool sameFailureRepeated = false;
	};

	std::vector<RoundRecord> rounds_;
	int sameFailureStreak_ = 0;
};

} // namespace agentos
