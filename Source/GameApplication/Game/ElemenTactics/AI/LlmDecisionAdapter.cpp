#include "LlmDecisionAdapter.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace ElemenTactics {

namespace {

std::optional<std::string> ExtractString(const std::string& source, const char* key){
	const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
	std::smatch match;
	if(!std::regex_search(source, match, pattern)) return std::nullopt;
	return match[1].str();
}

std::optional<int> ExtractInt(const std::string& source, const char* key){
	const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*(-?[0-9]+)");
	std::smatch match;
	if(!std::regex_search(source, match, pattern)) return std::nullopt;
	try{ return std::stoi(match[1].str()); }
	catch(...){ return std::nullopt; }
}

std::optional<double> ExtractDouble(const std::string& source, const char* key){
	const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
	std::smatch match;
	if(!std::regex_search(source, match, pattern)) return std::nullopt;
	try{ return std::stod(match[1].str()); }
	catch(...){ return std::nullopt; }
}

bool ContainsAction(const std::vector<GameAction>& legalActions, const GameAction& action){
	return std::find(legalActions.begin(), legalActions.end(), action) != legalActions.end();
}

std::string PieceSummary(const PublicPieceView& piece){
	std::ostringstream stream;
	stream << "piece=" << static_cast<int>(piece.id.slot)
		<< ",cell=" << (piece.cell ? std::to_string(*piece.cell) : "off")
		<< ",cards=" << piece.remainingCards;
	if(piece.ownedByViewer){
		stream << ",own_order=[";
		for(std::size_t index = 0; index < piece.visibleDeck.size(); ++index){
			if(index) stream << ',';
			stream << ToString(piece.visibleDeck[index]);
		}
		stream << ']';
	} else {
		stream << ",known={";
		for(std::size_t element = 0; element < ElementIndex(ElementType::Count); ++element){
			if(element) stream << ',';
			stream << ToString(static_cast<ElementType>(element)) << ':'
				<< piece.knowledge.knownPresentLowerBound[element];
		}
		stream << "},reordered=" << (piece.knowledge.wasReordered ? "true" : "false");
	}
	return stream.str();
}

} // namespace

std::string LlmDecisionAdapter::BuildPrompt(
	const PublicGameView& view,
	const std::vector<GameAction>& legalActions){
	std::ostringstream stream;
	stream << "You are the ElemenTactics opponent. Select exactly one listed legal action.\n"
		<< "Do not infer or claim access to hidden opponent deck order.\n"
		<< "Return one compact JSON object only; do not provide private chain-of-thought.\n"
		<< "viewer=" << ToString(view.viewer)
		<< ", actions_remaining=" << view.actionsRemaining << "\n";

	stream << "OWN PIECES\n";
	for(const PublicPieceView& piece : view.pieces[PlayerIndex(view.viewer)]){
		if(piece.alive) stream << PieceSummary(piece) << '\n';
	}
	stream << "OPPONENT PUBLIC PIECES\n";
	for(const PublicPieceView& piece : view.pieces[PlayerIndex(Opponent(view.viewer))]){
		if(piece.alive) stream << PieceSummary(piece) << '\n';
	}
	stream << "LEGAL ACTIONS\n";
	for(std::size_t index = 0; index < legalActions.size(); ++index){
		const GameAction& action = legalActions[index];
		stream << index << ": action_type="
			<< (action.type == ActionType::Scout ? "scout" : "move_or_battle")
			<< ",own_piece=" << static_cast<int>(action.actor.slot);
		if(action.type == ActionType::Scout){
			stream << ",target_piece=" << static_cast<int>(action.targetPiece->slot);
		} else {
			stream << ",target_cell=" << action.targetCell;
		}
		stream << '\n';
	}
	stream << "SCHEMA\n"
		<< "{\"action_type\":\"scout|move_or_battle\",\"own_piece\":0,"
		<< "\"target_piece\":0,\"target_cell\":0,"
		<< "\"public_intent\":\"short label\",\"threat_piece\":0,"
		<< "\"confidence\":0.0,\"public_reason\":\"short board-linked reason\"}";
	return stream.str();
}

std::optional<LlmDecision> LlmDecisionAdapter::ParseAndValidate(
	const PublicGameView& view,
	const std::vector<GameAction>& legalActions,
	const std::string& response,
	std::string* error){
	auto fail = [&](const std::string& message) -> std::optional<LlmDecision>{
		if(error) *error = message;
		return std::nullopt;
	};
	const auto actionType = ExtractString(response, "action_type");
	const auto ownPiece = ExtractInt(response, "own_piece");
	if(!actionType || !ownPiece || *ownPiece < 0 || *ownPiece >= 3){
		return fail("missing or invalid action_type/own_piece");
	}
	const PieceId actor{view.viewer, static_cast<std::uint8_t>(*ownPiece)};
	GameAction action;
	if(*actionType == "scout"){
		const auto targetPiece = ExtractInt(response, "target_piece");
		if(!targetPiece || *targetPiece < 0 || *targetPiece >= 3){
			return fail("missing or invalid target_piece");
		}
		action = GameAction::Scout(actor, PieceId{Opponent(view.viewer), static_cast<std::uint8_t>(*targetPiece)});
	} else if(*actionType == "move_or_battle"){
		const auto targetCell = ExtractInt(response, "target_cell");
		if(!targetCell || !IsBoardCell(*targetCell)) return fail("missing or invalid target_cell");
		action = GameAction::MoveOrBattle(actor, *targetCell);
	} else {
		return fail("unsupported action_type");
	}
	if(!ContainsAction(legalActions, action)){
		return fail("LLM selected an action outside the enumerated legal set");
	}

	LlmDecision decision;
	decision.action = action;
	decision.publicReasoning.currentGoal = SanitizePublicText(
		ExtractString(response, "public_intent").value_or("公開情報から安全な合法手を選ぶ"), 80);
	decision.publicReasoning.actionReason = SanitizePublicText(
		ExtractString(response, "public_reason").value_or("盤面と公開履歴に基づく選択"), 180);
	decision.publicReasoning.confidence = std::clamp(
		ExtractDouble(response, "confidence").value_or(0.5), 0.0, 1.0);
	if(const auto threat = ExtractInt(response, "threat_piece"); threat && *threat >= 0 && *threat < 3){
		decision.publicReasoning.highlightedPiece = PieceId{
			Opponent(view.viewer), static_cast<std::uint8_t>(*threat)};
		decision.publicReasoning.warningPiece = "P" +
			std::to_string(PlayerIndex(Opponent(view.viewer)) + 1) + "-" +
			std::to_string(*threat + 1);
	}
	return decision;
}

std::string LlmDecisionAdapter::SanitizePublicText(std::string text, std::size_t maxLength){
	text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char character){
		return character < 0x20 && character != '\t';
	}), text.end());
	if(text.size() > maxLength) text.resize(maxLength);
	return text;
}

} // namespace ElemenTactics
