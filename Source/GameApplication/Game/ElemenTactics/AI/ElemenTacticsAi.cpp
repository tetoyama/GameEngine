#include "ElemenTacticsAi.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace ElemenTactics {

namespace {

const PublicPieceView* FindPublicPiece(const PublicGameView& view, PieceId id){
	if(id.slot >= 3) return nullptr;
	return &view.pieces[PlayerIndex(id.owner)][id.slot];
}

const PublicPieceView* FindPublicPieceAt(const PublicGameView& view, int cell){
	if(!IsBoardCell(cell)) return nullptr;
	const auto& occupant = view.occupancy[static_cast<std::size_t>(cell)];
	return occupant ? FindPublicPiece(view, *occupant) : nullptr;
}

std::uint64_t Mix(std::uint64_t value){
	value += 0x9e3779b97f4a7c15ULL;
	value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
	value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
	return value ^ (value >> 31U);
}

double UnitNoise(std::uint64_t seed){
	return static_cast<double>(Mix(seed) & 0xFFFFFFULL) / static_cast<double>(0xFFFFFFULL);
}

std::uint64_t ActionFingerprint(const GameAction& action){
	std::uint64_t value = static_cast<std::uint64_t>(action.type);
	value = value * 131U + static_cast<std::uint64_t>(PlayerIndex(action.actor.owner));
	value = value * 131U + action.actor.slot;
	value = value * 131U + static_cast<std::uint64_t>(action.targetCell + 1);
	if(action.targetPiece){
		value = value * 131U + static_cast<std::uint64_t>(PlayerIndex(action.targetPiece->owner));
		value = value * 131U + action.targetPiece->slot;
	}
	return value;
}

double OutcomeValue(ElementType own, ElementType enemy){
	const BattleOutcome outcome = ElemenTacticsRules::ResolveMatchup(own, enemy);
	double value = outcome == BattleOutcome::AttackerWin ? 32.0 :
		(outcome == BattleOutcome::DefenderWin ? -38.0 : 2.0);
	if(enemy == ElementType::Light && own == ElementType::Dark) value += 180.0;
	if(enemy == ElementType::Dark && own != ElementType::Light && own != ElementType::Dark) value += 165.0;
	if(own == ElementType::Light && enemy == ElementType::Dark) value -= 240.0;
	if(own == ElementType::Dark && enemy != ElementType::Light && enemy != ElementType::Dark) value -= 220.0;
	return value;
}

std::string PieceLabel(PieceId piece){
	std::ostringstream stream;
	stream << (piece.owner == PlayerId::One ? "P1" : "P2") << '-' << static_cast<int>(piece.slot + 1);
	return stream.str();
}

std::pair<std::optional<PieceId>, double> HighestCandidate(const BeliefState& belief, ElementType element){
	std::optional<PieceId> best;
	double probability = -1.0;
	for(const PieceBelief& piece : belief.Pieces()){
		const double candidate = piece.frontProbability[ElementIndex(element)];
		if(candidate > probability){ probability = candidate; best = piece.piece; }
	}
	return {best, std::max(0.0, probability)};
}

} // namespace

BeliefState BeliefState::Build(const PublicGameView& view){
	BeliefState result;
	const PlayerId enemy = Opponent(view.viewer);
	std::array<int, ElementIndex(ElementType::Count)> globallyRemaining = RequiredDeckCounts;
	int totalEnemyCards = 0;
	for(const PublicPieceView& piece : view.pieces[PlayerIndex(enemy)]){
		totalEnemyCards += piece.remainingCards;
		for(std::size_t element = 0; element < globallyRemaining.size(); ++element){
			globallyRemaining[element] = std::max(0, globallyRemaining[element] - piece.knowledge.lostCounts[element]);
		}
	}
	for(const PublicPieceView& piece : view.pieces[PlayerIndex(enemy)]){
		if(!piece.alive || piece.remainingCards <= 0) continue;
		PieceBelief entry;
		entry.piece = piece.id;
		double sum = 0.0;
		for(std::size_t element = 0; element < entry.frontProbability.size(); ++element){
			double weight = totalEnemyCards > 0
				? static_cast<double>(globallyRemaining[element]) / static_cast<double>(totalEnemyCards)
				: 0.0;
			if(piece.knowledge.knownPresentLowerBound[element] > 0){
				weight += 0.85 * static_cast<double>(piece.knowledge.knownPresentLowerBound[element]) /
					static_cast<double>(piece.remainingCards);
			}
			entry.frontProbability[element] = std::max(0.0001, weight);
			sum += entry.frontProbability[element];
		}
		for(double& value : entry.frontProbability) value /= sum;
		for(const double probability : entry.frontProbability){
			entry.entropy -= probability * std::log2(probability);
		}
		result.m_pieces.push_back(entry);
	}
	return result;
}

const PieceBelief* BeliefState::Find(PieceId piece) const noexcept{
	const auto it = std::find_if(m_pieces.begin(), m_pieces.end(), [&](const PieceBelief& entry){
		return entry.piece == piece;
	});
	return it == m_pieces.end() ? nullptr : &*it;
}

std::optional<AiDecision> HeuristicAi::ChooseAction(
	const PublicGameView& view,
	const std::vector<GameAction>& legalActions,
	std::uint64_t seed){
	if(legalActions.empty() || view.currentPlayer != view.viewer || view.result.finished) return std::nullopt;
	const BeliefState belief = BeliefState::Build(view);
	const GameAction* bestAction = nullptr;
	double bestScore = -std::numeric_limits<double>::infinity();
	for(const GameAction& action : legalActions){
		const double score = ScoreAction(view, belief, action, seed);
		if(score > bestScore){ bestScore = score; bestAction = &action; }
	}
	if(!bestAction) return std::nullopt;

	AiDecision decision;
	decision.action = *bestAction;
	decision.score = bestScore;
	const auto [lightPiece, lightProbability] = HighestCandidate(belief, ElementType::Light);
	const auto [darkPiece, darkProbability] = HighestCandidate(belief, ElementType::Dark);
	decision.reasoning.currentGoal = bestAction->type == ActionType::Scout
		? "公開情報を増やし、相手の先頭順を崩す"
		: "重要カードを失う危険を抑えながら勝ち筋へ進む";
	decision.reasoning.lightCandidate = lightPiece
		? PieceLabel(*lightPiece) + " " + std::to_string(static_cast<int>(lightProbability * 100.0)) + "%"
		: "候補なし";
	decision.reasoning.darkCandidate = darkPiece
		? PieceLabel(*darkPiece) + " " + std::to_string(static_cast<int>(darkProbability * 100.0)) + "%"
		: "候補なし";
	decision.reasoning.warningPiece = darkPiece ? PieceLabel(*darkPiece) : "なし";
	decision.reasoning.highlightedPiece = bestAction->targetPiece;
	if(bestAction->type == ActionType::Scout){
		decision.reasoning.actionReason = "不確実性の高い駒を偵察し、双方の先頭を末尾へ送る";
	} else if(const PublicPieceView* target = FindPublicPieceAt(view, bestAction->targetCell);
		target && target->id.owner != view.viewer){
		decision.reasoning.actionReason = "公開情報に基づく期待勝率が高い対象へ戦闘を仕掛ける";
		decision.reasoning.highlightedPiece = target->id;
	} else if(bestAction->targetCell == CenterCellId){
		decision.reasoning.actionReason = "中央再編の機会を作り、読まれた順番を更新する";
	} else {
		decision.reasoning.actionReason = "危険な衝突を避けつつ盤面上の見せ方を変える";
	}
	decision.reasoning.confidence = std::clamp(0.5 + bestScore / 500.0, 0.05, 0.95);
	return decision;
}

std::vector<ElementType> HeuristicAi::ChooseCenterReorder(
	const PublicGameView& view,
	PieceId piece,
	std::uint64_t seed){
	const PublicPieceView* ownPiece = FindPublicPiece(view, piece);
	if(!ownPiece || !ownPiece->ownedByViewer) return {};
	std::vector<ElementType> order = ownPiece->visibleDeck;
	if(order.size() <= 1) return order;

	const BeliefState belief = BeliefState::Build(view);
	std::array<double, ElementIndex(ElementType::Count)> aggregate{};
	double totalWeight = 0.0;
	for(const PieceBelief& enemy : belief.Pieces()){
		const PublicPieceView* enemyView = FindPublicPiece(view, enemy.piece);
		const double weight = enemyView ? std::max(1, enemyView->remainingCards) : 1;
		totalWeight += weight;
		for(std::size_t element = 0; element < aggregate.size(); ++element){
			aggregate[element] += enemy.frontProbability[element] * weight;
		}
	}
	if(totalWeight > 0.0){ for(double& probability : aggregate) probability /= totalWeight; }
	auto cardScore = [&](ElementType card, std::size_t originalIndex){
		double score = 0.0;
		for(std::size_t enemy = 0; enemy < aggregate.size(); ++enemy){
			score += aggregate[enemy] * OutcomeValue(card, static_cast<ElementType>(enemy));
		}
		return score + UnitNoise(seed ^ (static_cast<std::uint64_t>(originalIndex) << 16U) ^ ElementIndex(card)) * 0.25;
	};
	std::vector<std::pair<ElementType, std::size_t>> indexed;
	indexed.reserve(order.size());
	for(std::size_t index = 0; index < order.size(); ++index) indexed.emplace_back(order[index], index);
	std::stable_sort(indexed.begin(), indexed.end(), [&](const auto& lhs, const auto& rhs){
		return cardScore(lhs.first, lhs.second) > cardScore(rhs.first, rhs.second);
	});
	for(std::size_t index = 0; index < order.size(); ++index) order[index] = indexed[index].first;
	return order;
}

double HeuristicAi::ScoreAction(
	const PublicGameView& view,
	const BeliefState& belief,
	const GameAction& action,
	std::uint64_t seed){
	const PublicPieceView* actor = FindPublicPiece(view, action.actor);
	if(!actor || actor->visibleDeck.empty()) return -100000.0;
	const ElementType ownFront = actor->visibleDeck.front();
	double score = 0.0;
	if(action.type == ActionType::Scout){
		const PieceBelief* targetBelief = action.targetPiece ? belief.Find(*action.targetPiece) : nullptr;
		score = 8.0 + (targetBelief ? targetBelief->entropy * 7.0 : 0.0);
		if(targetBelief){
			score += targetBelief->frontProbability[ElementIndex(ElementType::Light)] * 18.0;
			score += targetBelief->frontProbability[ElementIndex(ElementType::Dark)] * 18.0;
		}
		if(IsCritical(ownFront)) score -= 5.0;
	} else {
		const PublicPieceView* target = FindPublicPieceAt(view, action.targetCell);
		if(target && target->id.owner != view.viewer){
			const PieceBelief* targetBelief = belief.Find(target->id);
			if(targetBelief){
				for(std::size_t enemy = 0; enemy < targetBelief->frontProbability.size(); ++enemy){
					score += targetBelief->frontProbability[enemy] * OutcomeValue(ownFront, static_cast<ElementType>(enemy));
				}
			}
			if(target->remainingCards == 1) score += 25.0;
		} else if(action.targetCell == CenterCellId && actor->reorderEligibleOnArrival){
			score += 24.0;
		} else {
			score += 0.5;
		}
	}
	score += (UnitNoise(seed ^ ActionFingerprint(action)) - 0.5) * 0.75;
	return score;
}

} // namespace ElemenTactics
