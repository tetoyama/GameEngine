#include "ElemenTacticsRules.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace ElemenTactics {

namespace {

constexpr std::array<std::array<BattleOutcome, ElementIndex(ElementType::Count)>, ElementIndex(ElementType::Count)> MatchupTable{{
	// Defender: Fire, Water, Wood, Dark, Light
	std::array{BattleOutcome::Draw, BattleOutcome::DefenderWin, BattleOutcome::AttackerWin, BattleOutcome::AttackerWin, BattleOutcome::DefenderWin},
	std::array{BattleOutcome::AttackerWin, BattleOutcome::Draw, BattleOutcome::DefenderWin, BattleOutcome::AttackerWin, BattleOutcome::DefenderWin},
	std::array{BattleOutcome::DefenderWin, BattleOutcome::AttackerWin, BattleOutcome::Draw, BattleOutcome::AttackerWin, BattleOutcome::DefenderWin},
	std::array{BattleOutcome::DefenderWin, BattleOutcome::DefenderWin, BattleOutcome::DefenderWin, BattleOutcome::Draw, BattleOutcome::AttackerWin},
	std::array{BattleOutcome::AttackerWin, BattleOutcome::AttackerWin, BattleOutcome::AttackerWin, BattleOutcome::DefenderWin, BattleOutcome::Draw}
}};

std::array<int, ElementIndex(ElementType::Count)> CountElements(const auto& range){
	std::array<int, ElementIndex(ElementType::Count)> counts{};
	for(const ElementType element : range){
		if(ElementIndex(element) >= counts.size()) continue;
		++counts[ElementIndex(element)];
	}
	return counts;
}

void AppendEvent(GameState& state, PublicEvent event){
	event.actionSerial = state.actionSerial;
	event.actingPlayer = state.currentPlayer;
	state.publicKnowledge.history.push_back(std::move(event));
}

bool SameMultiset(const std::deque<ElementType>& current, const std::vector<ElementType>& requested){
	return current.size() == requested.size() && CountElements(current) == CountElements(requested);
}

} // namespace

const char* ToString(ElementType element) noexcept{
	switch(element){
	case ElementType::Fire: return "fire";
	case ElementType::Water: return "water";
	case ElementType::Wood: return "wood";
	case ElementType::Dark: return "dark";
	case ElementType::Light: return "light";
	default: return "unknown";
	}
}

const char* ToString(PlayerId player) noexcept{
	return player == PlayerId::One ? "player_one" : "player_two";
}

BattleOutcome ElemenTacticsRules::ResolveMatchup(ElementType attacker, ElementType defender) noexcept{
	const std::size_t a = ElementIndex(attacker);
	const std::size_t d = ElementIndex(defender);
	if(a >= MatchupTable.size() || d >= MatchupTable[a].size()) return BattleOutcome::Draw;
	return MatchupTable[a][d];
}

bool ElemenTacticsRules::ValidateDeckSetup(const DeckSetup& setup, std::string* error){
	for(std::size_t player = 0; player < setup.decks.size(); ++player){
		std::array<int, ElementIndex(ElementType::Count)> counts{};
		int total = 0;
		for(const auto& pieceDeck : setup.decks[player]){
			for(const ElementType element : pieceDeck){
				const std::size_t index = ElementIndex(element);
				if(index >= counts.size()){
					if(error) *error = "deck contains an invalid element";
					return false;
				}
				++counts[index];
				++total;
			}
		}
		if(total != 8){
			if(error) *error = "each player must assign exactly eight cards";
			return false;
		}
		if(counts != RequiredDeckCounts){
			if(error) *error = "deck multiset must be 2 fire, 2 water, 2 wood, 1 dark, 1 light";
			return false;
		}
	}
	return true;
}

GameState ElemenTacticsRules::CreateInitialState(const DeckSetup& setup, PlayerId firstPlayer){
	std::string error;
	if(!ValidateDeckSetup(setup, &error)) throw std::invalid_argument(error);

	GameState state;
	state.currentPlayer = firstPlayer;
	state.actionsRemaining = 2;
	state.players[PlayerIndex(firstPlayer)].ownTurnNumber = 1;
	for(std::size_t playerIndex = 0; playerIndex < state.players.size(); ++playerIndex){
		const PlayerId player = static_cast<PlayerId>(playerIndex);
		const auto& starts = player == PlayerId::One ? PlayerOneStartCells : PlayerTwoStartCells;
		for(std::size_t slot = 0; slot < 3; ++slot){
			PieceState piece;
			piece.id = PieceId{player, static_cast<std::uint8_t>(slot)};
			piece.deck.assign(setup.decks[playerIndex][slot].begin(), setup.decks[playerIndex][slot].end());
			if(!piece.deck.empty()){
				piece.cell = starts[slot];
				state.occupancy[static_cast<std::size_t>(*piece.cell)] = piece.id;
			}
			state.players[playerIndex].pieces[slot] = std::move(piece);
		}
	}
	if(!ValidateInvariant(state, &error)) throw std::logic_error(error);
	return state;
}

const PieceState* ElemenTacticsRules::FindPiece(const GameState& state, PieceId id) noexcept{
	if(id.slot >= 3) return nullptr;
	return &state.players[PlayerIndex(id.owner)].pieces[id.slot];
}

PieceState* ElemenTacticsRules::FindPiece(GameState& state, PieceId id) noexcept{
	if(id.slot >= 3) return nullptr;
	return &state.players[PlayerIndex(id.owner)].pieces[id.slot];
}

std::vector<GameAction> ElemenTacticsRules::GenerateLegalActions(const GameState& state){
	std::vector<GameAction> actions;
	if(state.result.finished || state.pendingReorder || state.actionsRemaining <= 0) return actions;
	const auto& ownPieces = state.players[PlayerIndex(state.currentPlayer)].pieces;
	const auto& enemyPieces = state.players[PlayerIndex(Opponent(state.currentPlayer))].pieces;
	for(const PieceState& actor : ownPieces){
		if(!actor.IsAlive()) continue;
		for(int cell = 0; cell < BoardCellCount; ++cell){
			const auto& occupant = state.occupancy[static_cast<std::size_t>(cell)];
			if(occupant && occupant->owner == state.currentPlayer) continue;
			actions.push_back(GameAction::MoveOrBattle(actor.id, cell));
		}
		for(const PieceState& target : enemyPieces){
			if(target.IsAlive()) actions.push_back(GameAction::Scout(actor.id, target.id));
		}
	}
	return actions;
}

bool ElemenTacticsRules::IsLegalAction(const GameState& state, const GameAction& action, std::string* error){
	auto fail = [&](const char* message){ if(error) *error = message; return false; };
	if(state.result.finished) return fail("the match is already finished");
	if(state.pendingReorder) return fail("center reorder must be resolved before another action");
	if(state.actionsRemaining <= 0) return fail("no actions remain");
	if(action.actor.owner != state.currentPlayer) return fail("actor does not belong to the active player");
	const PieceState* actor = FindPiece(state, action.actor);
	if(!actor || !actor->IsAlive()) return fail("actor is not alive on the board");
	if(action.type == ActionType::MoveOrBattle){
		if(!IsBoardCell(action.targetCell)) return fail("target cell is outside the board");
		const auto& occupant = state.occupancy[static_cast<std::size_t>(action.targetCell)];
		if(occupant && occupant->owner == state.currentPlayer) return fail("a friendly piece occupies the target cell");
		return true;
	}
	if(action.type == ActionType::Scout){
		if(!action.targetPiece) return fail("scout requires an enemy target piece");
		if(action.targetPiece->owner == state.currentPlayer) return fail("scout target must be an enemy piece");
		const PieceState* target = FindPiece(state, *action.targetPiece);
		if(!target || !target->IsAlive()) return fail("scout target is not alive on the board");
		return true;
	}
	return fail("unknown action type");
}

ActionResult ElemenTacticsRules::ApplyAction(GameState& state, const GameAction& action){
	ActionResult result;
	if(!IsLegalAction(state, action, &result.error)) return result;
	++state.actionSerial;
	PieceState* actor = FindPiece(state, action.actor);
	assert(actor && actor->IsAlive());

	if(action.type == ActionType::Scout){
		PieceState* target = FindPiece(state, *action.targetPiece);
		assert(target && target->IsAlive());
		const ElementType actorElement = actor->deck.front();
		const ElementType targetElement = target->deck.front();
		RecordReveal(state, actor->id, actorElement);
		RecordReveal(state, target->id, targetElement);
		actor->deck.pop_front(); actor->deck.push_back(actorElement);
		target->deck.pop_front(); target->deck.push_back(targetElement);
		RecordRotation(state, actor->id, actorElement);
		RecordRotation(state, target->id, targetElement);
		PublicEvent event;
		event.type = PublicEventType::Scout;
		event.primaryPiece = actor->id;
		event.secondaryPiece = target->id;
		event.primaryElement = actorElement;
		event.secondaryElement = targetElement;
		AppendEvent(state, std::move(event));
		result.scout = ScoutResolution{actor->id, target->id, actorElement, targetElement};
		result.applied = true;
		FinishAction(state, result);
		return result;
	}

	const int oldActorCell = *actor->cell;
	const auto targetOccupant = state.occupancy[static_cast<std::size_t>(action.targetCell)];
	if(!targetOccupant){
		state.occupancy[static_cast<std::size_t>(oldActorCell)].reset();
		state.occupancy[static_cast<std::size_t>(action.targetCell)] = actor->id;
		actor->cell = action.targetCell;
		if(actor->everReordered && oldActorCell == CenterCellId && action.targetCell != CenterCellId){
			actor->exitedCenterSinceReorder = true;
		}
		PublicEvent event;
		event.type = PublicEventType::Move;
		event.primaryPiece = actor->id;
		event.cell = action.targetCell;
		AppendEvent(state, std::move(event));
		if(action.targetCell == CenterCellId) TryOpenCenterReorder(state, *actor);
		result.openedCenterReorder = state.pendingReorder.has_value();
		result.applied = true;
		FinishAction(state, result);
		return result;
	}

	PieceState* defender = FindPiece(state, *targetOccupant);
	assert(defender && defender->IsAlive());
	const int defenderCell = *defender->cell;
	const ElementType attackerElement = actor->deck.front();
	const ElementType defenderElement = defender->deck.front();
	const BattleOutcome outcome = ResolveMatchup(attackerElement, defenderElement);
	RecordReveal(state, actor->id, attackerElement);
	RecordReveal(state, defender->id, defenderElement);

	BattleResolution battle;
	battle.attacker = actor->id;
	battle.defender = defender->id;
	battle.attackerElement = attackerElement;
	battle.defenderElement = defenderElement;
	battle.outcome = outcome;
	PublicEvent battleEvent;
	battleEvent.type = PublicEventType::Battle;
	battleEvent.primaryPiece = actor->id;
	battleEvent.secondaryPiece = defender->id;
	battleEvent.primaryElement = attackerElement;
	battleEvent.secondaryElement = defenderElement;
	battleEvent.battleOutcome = outcome;
	battleEvent.cell = defenderCell;
	AppendEvent(state, std::move(battleEvent));

	if(outcome == BattleOutcome::Draw){
		actor->deck.pop_front(); actor->deck.push_back(attackerElement);
		defender->deck.pop_front(); defender->deck.push_back(defenderElement);
		RecordRotation(state, actor->id, attackerElement);
		RecordRotation(state, defender->id, defenderElement);
	} else {
		PieceState* winner = outcome == BattleOutcome::AttackerWin ? actor : defender;
		PieceState* loser = outcome == BattleOutcome::AttackerWin ? defender : actor;
		const ElementType winnerElement = winner->deck.front();
		const ElementType loserElement = loser->deck.front();
		winner->deck.pop_front(); winner->deck.push_back(winnerElement);
		loser->deck.pop_front();
		RecordRotation(state, winner->id, winnerElement);
		RecordLoss(state, loser->id, loserElement);
		battle.lostElement = loserElement;
		if(loser->deck.empty()){
			const int loserCell = *loser->cell;
			state.occupancy[static_cast<std::size_t>(loserCell)].reset();
			loser->cell.reset();
			battle.defeatedPiece = loser->id;
			PublicEvent defeatedEvent;
			defeatedEvent.type = PublicEventType::PieceDefeated;
			defeatedEvent.primaryPiece = loser->id;
			defeatedEvent.cell = loserCell;
			AppendEvent(state, std::move(defeatedEvent));
			if(outcome == BattleOutcome::AttackerWin){
				state.occupancy[static_cast<std::size_t>(oldActorCell)].reset();
				state.occupancy[static_cast<std::size_t>(defenderCell)] = actor->id;
				actor->cell = defenderCell;
				battle.attackerAdvanced = true;
				if(actor->everReordered && oldActorCell == CenterCellId && defenderCell != CenterCellId){
					actor->exitedCenterSinceReorder = true;
				}
			}
		}
		if(IsCritical(loserElement)) MarkGameSet(state, loser->id.owner, loserElement, winner->id);
	}
	if(!state.result.finished && battle.attackerAdvanced && actor->cell == CenterCellId){
		TryOpenCenterReorder(state, *actor);
	}
	result.battle = battle;
	result.openedCenterReorder = state.pendingReorder.has_value();
	result.applied = true;
	FinishAction(state, result);
	return result;
}

ReorderResult ElemenTacticsRules::ResolvePendingReorder(
	GameState& state,
	const std::optional<std::vector<ElementType>>& newOrder){
	ReorderResult result;
	if(state.result.finished){ result.error = "the match is already finished"; return result; }
	if(!state.pendingReorder){ result.error = "there is no pending center reorder"; return result; }
	PieceState* piece = FindPiece(state, state.pendingReorder->piece);
	if(!piece || !piece->IsAlive() || piece->cell != CenterCellId){
		result.error = "pending reorder piece is no longer alive at the center";
		return result;
	}
	if(newOrder){
		if(!SameMultiset(piece->deck, *newOrder)){
			result.error = "reorder must preserve the exact remaining card multiset";
			return result;
		}
		piece->deck.assign(newOrder->begin(), newOrder->end());
		piece->everReordered = true;
		piece->exitedCenterSinceReorder = false;
		piece->lastReorderOwnerTurn = state.players[PlayerIndex(piece->id.owner)].ownTurnNumber;
		auto& knowledge = state.publicKnowledge.pieces[PlayerIndex(piece->id.owner)][piece->id.slot];
		++knowledge.orderRevision;
		knowledge.orderInvalidatedByReorder = true;
		knowledge.wasReordered = true;
		PublicEvent event;
		event.type = PublicEventType::CenterReordered;
		event.primaryPiece = piece->id;
		event.cell = CenterCellId;
		AppendEvent(state, std::move(event));
		result.reordered = true;
	}
	state.pendingReorder.reset();
	result.applied = true;
	if(state.actionsRemaining == 0) AdvanceTurn(state, &result.turnAdvanced);
	return result;
}

PublicGameView ElemenTacticsRules::BuildPublicView(const GameState& state, PlayerId viewer){
	PublicGameView view;
	view.viewer = viewer;
	view.currentPlayer = state.currentPlayer;
	view.actionsRemaining = state.actionsRemaining;
	view.viewerOwnTurnNumber = state.players[PlayerIndex(viewer)].ownTurnNumber;
	view.occupancy = state.occupancy;
	view.history = state.publicKnowledge.history;
	view.pendingReorder = state.pendingReorder;
	view.result = state.result;
	for(std::size_t playerIndex = 0; playerIndex < state.players.size(); ++playerIndex){
		for(std::size_t slot = 0; slot < 3; ++slot){
			const PieceState& source = state.players[playerIndex].pieces[slot];
			PublicPieceView& destination = view.pieces[playerIndex][slot];
			destination.id = source.id;
			destination.cell = source.cell;
			destination.remainingCards = static_cast<int>(source.deck.size());
			destination.alive = source.IsAlive();
			destination.ownedByViewer = source.id.owner == viewer;
			destination.knowledge = state.publicKnowledge.pieces[playerIndex][slot];
			if(destination.ownedByViewer){
				destination.visibleDeck.assign(source.deck.begin(), source.deck.end());
				destination.reorderEligibleOnArrival = CanReorderOnArrival(state, source);
			}
		}
	}
	return view;
}

bool ElemenTacticsRules::ValidateInvariant(const GameState& state, std::string* error){
	auto fail = [&](const std::string& message){ if(error) *error = message; return false; };
	if(state.actionsRemaining < 0 || state.actionsRemaining > 2) return fail("actionsRemaining is outside 0..2");
	std::array<bool, BoardCellCount> occupied{};
	for(const PlayerState& player : state.players){
		for(const PieceState& piece : player.pieces){
			if(piece.deck.empty() != !piece.cell.has_value()) return fail("empty decks must be off-board and live decks must be on-board");
			if(piece.cell){
				if(!IsBoardCell(*piece.cell)) return fail("piece cell is outside the board");
				if(occupied[static_cast<std::size_t>(*piece.cell)]) return fail("two pieces occupy one cell");
				occupied[static_cast<std::size_t>(*piece.cell)] = true;
				if(state.occupancy[static_cast<std::size_t>(*piece.cell)] != piece.id) return fail("piece and occupancy map disagree");
			}
		}
	}
	for(int cell = 0; cell < BoardCellCount; ++cell){
		const auto& occupant = state.occupancy[static_cast<std::size_t>(cell)];
		if(!occupant) continue;
		const PieceState* piece = FindPiece(state, *occupant);
		if(!piece || piece->cell != cell || piece->deck.empty()) return fail("occupancy references an invalid piece");
	}
	if(state.pendingReorder){
		const PieceState* piece = FindPiece(state, state.pendingReorder->piece);
		if(!piece || piece->cell != CenterCellId || !piece->IsAlive()) return fail("pending reorder is not attached to a live center piece");
	}
	return true;
}

bool ElemenTacticsRules::CanReorderOnArrival(const GameState& state, const PieceState& piece) noexcept{
	if(!piece.everReordered) return true;
	if(!piece.exitedCenterSinceReorder) return false;
	const int currentOwnerTurn = state.players[PlayerIndex(piece.id.owner)].ownTurnNumber;
	return currentOwnerTurn >= piece.lastReorderOwnerTurn + 2;
}

void ElemenTacticsRules::TryOpenCenterReorder(GameState& state, PieceState& piece){
	if(state.result.finished || piece.cell != CenterCellId || !CanReorderOnArrival(state, piece)) return;
	state.pendingReorder = PendingReorder{piece.id};
}

void ElemenTacticsRules::FinishAction(GameState& state, ActionResult& result){
	if(state.result.finished){
		state.actionsRemaining = 0;
		state.pendingReorder.reset();
		return;
	}
	--state.actionsRemaining;
	if(state.pendingReorder) return;
	if(state.actionsRemaining == 0) AdvanceTurn(state, &result.turnAdvanced);
}

void ElemenTacticsRules::AdvanceTurn(GameState& state, bool* advanced){
	state.currentPlayer = Opponent(state.currentPlayer);
	++state.players[PlayerIndex(state.currentPlayer)].ownTurnNumber;
	state.actionsRemaining = 2;
	if(advanced) *advanced = true;
}

void ElemenTacticsRules::RecordReveal(GameState& state, PieceId piece, ElementType element){
	auto& knowledge = state.publicKnowledge.pieces[PlayerIndex(piece.owner)][piece.slot];
	knowledge.lastRevealed = element;
	knowledge.knownPresentLowerBound[ElementIndex(element)] = std::max(knowledge.knownPresentLowerBound[ElementIndex(element)], 1);
}

void ElemenTacticsRules::RecordLoss(GameState& state, PieceId piece, ElementType element){
	auto& knowledge = state.publicKnowledge.pieces[PlayerIndex(piece.owner)][piece.slot];
	++knowledge.lostCounts[ElementIndex(element)];
	knowledge.knownPresentLowerBound[ElementIndex(element)] = std::max(0, knowledge.knownPresentLowerBound[ElementIndex(element)] - 1);
	PublicEvent event;
	event.type = PublicEventType::CardLost;
	event.primaryPiece = piece;
	event.primaryElement = element;
	AppendEvent(state, std::move(event));
}

void ElemenTacticsRules::RecordRotation(GameState& state, PieceId piece, ElementType element){
	PublicEvent event;
	event.type = PublicEventType::CardRotated;
	event.primaryPiece = piece;
	event.primaryElement = element;
	AppendEvent(state, std::move(event));
}

void ElemenTacticsRules::MarkGameSet(
	GameState& state,
	PlayerId loser,
	ElementType lostElement,
	std::optional<PieceId> decisivePiece){
	state.result.finished = true;
	state.result.loser = loser;
	state.result.winner = Opponent(loser);
	state.result.reason = lostElement == ElementType::Light ? GameEndReason::KingLost : GameEndReason::AssassinLost;
	state.result.decisivePiece = decisivePiece;
	state.result.lostElement = lostElement;
	PublicEvent event;
	event.type = PublicEventType::GameSet;
	event.primaryPiece = decisivePiece;
	event.primaryElement = lostElement;
	AppendEvent(state, std::move(event));
}

} // namespace ElemenTactics
