#include "DeckSetupModel.h"

#include <algorithm>

namespace ElemenTactics {

namespace {

std::vector<ElementType> StandardCards(){
	return {
		ElementType::Fire, ElementType::Fire,
		ElementType::Water, ElementType::Water,
		ElementType::Wood, ElementType::Wood,
		ElementType::Dark, ElementType::Light
	};
}

bool Fail(std::string* error, const char* message){
	if(error) *error = message;
	return false;
}

} // namespace

DeckSetupModel DeckSetupModel::BalancedDefault(){
	const auto cards = StandardCards();
	return DeckSetupModel({{
		{cards.begin(), cards.begin() + 3},
		{cards.begin() + 3, cards.begin() + 6},
		{cards.begin() + 6, cards.end()}
	}});
}

DeckSetupModel DeckSetupModel::ConcentratedDefault(){
	return DeckSetupModel({{StandardCards(), {}, {}}});
}

bool DeckSetupModel::MoveCard(
	std::size_t sourceSlot,
	std::size_t sourceIndex,
	std::size_t destinationSlot,
	std::size_t destinationIndex,
	std::string* error){
	if(sourceSlot >= m_decks.size() || destinationSlot >= m_decks.size()){
		return Fail(error, "piece slot is outside 0..2");
	}
	auto& source = m_decks[sourceSlot];
	auto& destination = m_decks[destinationSlot];
	if(sourceIndex >= source.size()) return Fail(error, "source card index is invalid");
	if(destinationIndex > destination.size()) return Fail(error, "destination card index is invalid");
	if(sourceSlot == destinationSlot){
		return ReorderCard(sourceSlot, sourceIndex, destinationIndex, error);
	}
	const ElementType card = source[sourceIndex];
	source.erase(source.begin() + static_cast<std::ptrdiff_t>(sourceIndex));
	destination.insert(destination.begin() + static_cast<std::ptrdiff_t>(destinationIndex), card);
	return true;
}

bool DeckSetupModel::ReorderCard(
	std::size_t slot,
	std::size_t sourceIndex,
	std::size_t destinationIndex,
	std::string* error){
	if(slot >= m_decks.size()) return Fail(error, "piece slot is outside 0..2");
	auto& deck = m_decks[slot];
	if(sourceIndex >= deck.size()) return Fail(error, "source card index is invalid");
	if(destinationIndex > deck.size()) return Fail(error, "destination card index is invalid");
	if(sourceIndex == destinationIndex || sourceIndex + 1 == destinationIndex) return true;
	const ElementType card = deck[sourceIndex];
	deck.erase(deck.begin() + static_cast<std::ptrdiff_t>(sourceIndex));
	if(destinationIndex > sourceIndex) --destinationIndex;
	deck.insert(deck.begin() + static_cast<std::ptrdiff_t>(destinationIndex), card);
	return true;
}

bool DeckSetupModel::Validate(std::string* error) const{
	DeckSetup setup;
	setup.decks[0] = m_decks;
	setup.decks[1] = m_decks;
	return ElemenTacticsRules::ValidateDeckSetup(setup, error);
}

} // namespace ElemenTactics
