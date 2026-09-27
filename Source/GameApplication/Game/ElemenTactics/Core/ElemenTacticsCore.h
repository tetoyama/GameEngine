#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace ElemenTactics {

enum class ElementType : std::uint8_t {
	Fire = 0,
	Water,
	Wood,
	Dark,
	Light,
	Count
};

enum class PlayerId : std::uint8_t {
	One = 0,
	Two = 1
};

constexpr std::size_t ElementIndex(ElementType value){
	return static_cast<std::size_t>(value);
}

constexpr std::size_t PlayerIndex(PlayerId value){
	return static_cast<std::size_t>(value);
}

constexpr PlayerId Opponent(PlayerId value){
	return value == PlayerId::One ? PlayerId::Two : PlayerId::One;
}

struct PieceId {
	PlayerId owner = PlayerId::One;
	std::uint8_t slot = 0;

	friend constexpr bool operator==(const PieceId&, const PieceId&) = default;
};

struct BoardCell {
	int id = -1;
	int q = 0;
	int r = 0;
	bool center = false;
};

inline constexpr int BoardCellCount = 19;
inline constexpr int CenterCellId = 9;

inline constexpr std::array<BoardCell, BoardCellCount> BoardCells{{
	{0, 0, -2, false}, {1, 1, -2, false}, {2, 2, -2, false},
	{3, -1, -1, false}, {4, 0, -1, false}, {5, 1, -1, false}, {6, 2, -1, false},
	{7, -2, 0, false}, {8, -1, 0, false}, {9, 0, 0, true}, {10, 1, 0, false}, {11, 2, 0, false},
	{12, -2, 1, false}, {13, -1, 1, false}, {14, 0, 1, false}, {15, 1, 1, false},
	{16, -2, 2, false}, {17, -1, 2, false}, {18, 0, 2, false}
}};

inline constexpr std::array<int, 3> PlayerOneStartCells{{0, 1, 2}};
inline constexpr std::array<int, 3> PlayerTwoStartCells{{18, 17, 16}};
inline constexpr std::array<int, ElementIndex(ElementType::Count)> RequiredDeckCounts{{2, 2, 2, 1, 1}};

constexpr bool IsBoardCell(int cell){
	return cell >= 0 && cell < BoardCellCount;
}

constexpr bool IsCritical(ElementType element){
	return element == ElementType::Light || element == ElementType::Dark;
}

const char* ToString(ElementType element) noexcept;
const char* ToString(PlayerId player) noexcept;

struct DeckSetup {
	std::array<std::array<std::vector<ElementType>, 3>, 2> decks;
};

enum class ActionType : std::uint8_t {
	MoveOrBattle,
	Scout
};

struct GameAction {
	ActionType type = ActionType::MoveOrBattle;
	PieceId actor{};
	int targetCell = -1;
	std::optional<PieceId> targetPiece;

	static GameAction MoveOrBattle(PieceId actor, int targetCell){
		GameAction action;
		action.type = ActionType::MoveOrBattle;
		action.actor = actor;
		action.targetCell = targetCell;
		return action;
	}

	static GameAction Scout(PieceId actor, PieceId target){
		GameAction action;
		action.type = ActionType::Scout;
		action.actor = actor;
		action.targetPiece = target;
		return action;
	}

	friend bool operator==(const GameAction&, const GameAction&) = default;
};

enum class BattleOutcome : std::uint8_t {
	AttackerWin,
	DefenderWin,
	Draw
};

enum class GameEndReason : std::uint8_t {
	None,
	KingLost,
	AssassinLost
};

enum class PublicEventType : std::uint8_t {
	Move,
	Battle,
	Scout,
	CardLost,
	CardRotated,
	PieceDefeated,
	CenterReordered,
	GameSet
};

struct PublicEvent {
	PublicEventType type = PublicEventType::Move;
	std::uint64_t actionSerial = 0;
	PlayerId actingPlayer = PlayerId::One;
	std::optional<PieceId> primaryPiece;
	std::optional<PieceId> secondaryPiece;
	std::optional<ElementType> primaryElement;
	std::optional<ElementType> secondaryElement;
	std::optional<BattleOutcome> battleOutcome;
	int cell = -1;
};

struct PiecePublicKnowledge {
	std::array<int, ElementIndex(ElementType::Count)> knownPresentLowerBound{};
	std::array<int, ElementIndex(ElementType::Count)> lostCounts{};
	std::optional<ElementType> lastRevealed;
	std::uint32_t orderRevision = 0;
	bool orderInvalidatedByReorder = false;
	bool wasReordered = false;
};

struct PublicKnowledge {
	std::array<std::array<PiecePublicKnowledge, 3>, 2> pieces{};
	std::vector<PublicEvent> history;
};

struct PieceState {
	PieceId id{};
	std::deque<ElementType> deck;
	std::optional<int> cell;
	bool everReordered = false;
	bool exitedCenterSinceReorder = false;
	int lastReorderOwnerTurn = -1000000;

	bool IsAlive() const noexcept{
		return cell.has_value() && !deck.empty();
	}
};

struct PlayerState {
	std::array<PieceState, 3> pieces;
	int ownTurnNumber = 0;
};

struct GameResult {
	bool finished = false;
	PlayerId winner = PlayerId::One;
	PlayerId loser = PlayerId::Two;
	GameEndReason reason = GameEndReason::None;
	std::optional<PieceId> decisivePiece;
	std::optional<ElementType> lostElement;
};

struct PendingReorder {
	PieceId piece{};
};

struct GameState {
	std::array<PlayerState, 2> players;
	std::array<std::optional<PieceId>, BoardCellCount> occupancy;
	PlayerId currentPlayer = PlayerId::One;
	int actionsRemaining = 2;
	std::uint64_t actionSerial = 0;
	PublicKnowledge publicKnowledge;
	std::optional<PendingReorder> pendingReorder;
	GameResult result;
};

struct BattleResolution {
	PieceId attacker{};
	PieceId defender{};
	ElementType attackerElement = ElementType::Fire;
	ElementType defenderElement = ElementType::Fire;
	BattleOutcome outcome = BattleOutcome::Draw;
	std::optional<PieceId> defeatedPiece;
	std::optional<ElementType> lostElement;
	bool attackerAdvanced = false;
};

struct ScoutResolution {
	PieceId actor{};
	PieceId target{};
	ElementType actorElement = ElementType::Fire;
	ElementType targetElement = ElementType::Fire;
};

struct ActionResult {
	bool applied = false;
	std::string error;
	std::optional<BattleResolution> battle;
	std::optional<ScoutResolution> scout;
	bool openedCenterReorder = false;
	bool turnAdvanced = false;
};

struct ReorderResult {
	bool applied = false;
	bool reordered = false;
	std::string error;
	bool turnAdvanced = false;
};

struct PublicPieceView {
	PieceId id{};
	std::optional<int> cell;
	int remainingCards = 0;
	bool alive = false;
	bool ownedByViewer = false;
	std::vector<ElementType> visibleDeck;
	PiecePublicKnowledge knowledge;
	bool reorderEligibleOnArrival = false;
};

struct PublicGameView {
	PlayerId viewer = PlayerId::One;
	PlayerId currentPlayer = PlayerId::One;
	int actionsRemaining = 0;
	int viewerOwnTurnNumber = 0;
	std::array<std::array<PublicPieceView, 3>, 2> pieces;
	std::array<std::optional<PieceId>, BoardCellCount> occupancy;
	std::vector<PublicEvent> history;
	std::optional<PendingReorder> pendingReorder;
	GameResult result;
};

} // namespace ElemenTactics
