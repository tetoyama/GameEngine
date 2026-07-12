#pragma once

#include "../Rules/ElemenTacticsRules.h"

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace ElemenTactics {

class DeckSetupModel final {
public:
	static DeckSetupModel BalancedDefault();
	static DeckSetupModel ConcentratedDefault();

	const std::array<std::vector<ElementType>, 3>& Decks() const noexcept{ return m_decks; }
	std::array<std::vector<ElementType>, 3>& DecksForTesting() noexcept{ return m_decks; }

	bool MoveCard(
		std::size_t sourceSlot,
		std::size_t sourceIndex,
		std::size_t destinationSlot,
		std::size_t destinationIndex,
		std::string* error = nullptr);
	bool ReorderCard(
		std::size_t slot,
		std::size_t sourceIndex,
		std::size_t destinationIndex,
		std::string* error = nullptr);
	bool Validate(std::string* error = nullptr) const;

private:
	explicit DeckSetupModel(std::array<std::vector<ElementType>, 3> decks)
		: m_decks(std::move(decks)){}

	std::array<std::vector<ElementType>, 3> m_decks;
};

} // namespace ElemenTactics
