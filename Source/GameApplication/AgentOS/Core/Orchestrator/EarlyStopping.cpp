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
	if (sameFailureRepeated) {
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
		return StopDecision{true, "same failure repeated 3 or more times"};
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
