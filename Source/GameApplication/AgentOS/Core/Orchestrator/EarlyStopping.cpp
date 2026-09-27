// =======================================================================
//
// EarlyStopping.cpp
//
// =======================================================================
#include "EarlyStopping.h"

namespace agentos {

void EarlyStopping::RecordRound(int newEvidenceCount, LogicNodeId topHypothesis,
                                 int unresolvedContradictions, bool sameFailureRepeated) {
	rounds_.push_back(RoundRecord{newEvidenceCount, topHypothesis, unresolvedContradictions, sameFailureRepeated});

	// 大規模調査では「まだ全経路を追えていない」等の同じ高レベル失敗が、
	// 新Evidenceを積み上げている途中でも複数ラウンド継続し得る。
	// 進展があるラウンドを停滞として数えると、固定3ラウンド制限と同じになる。
	if (sameFailureRepeated && newEvidenceCount == 0) {
		++sameFailureStreak_;
	} else {
		sameFailureStreak_ = 0;
	}
}

EarlyStopping::StopDecision EarlyStopping::Evaluate(const BudgetTracker& budget) const {
	if (budget.RemainingRatio() < 0.2) {
		return StopDecision{true, "budget remaining ratio below 0.2"};
	}

	if (sameFailureStreak_ >= 3) {
		return StopDecision{true, "same failure repeated without new evidence 3 or more times"};
	}

	if (rounds_.size() >= 2) {
		const RoundRecord& last = rounds_.back();
		const RoundRecord& prev = rounds_[rounds_.size() - 2];
		if (last.newEvidenceCount == 0 && prev.newEvidenceCount == 0 &&
		    last.topHypothesis == prev.topHypothesis) {
			return StopDecision{true, "no new evidence in the last 2 rounds and top hypothesis unchanged"};
		}
	}

	return StopDecision{false, ""};
}

} // namespace agentos
