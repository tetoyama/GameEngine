#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace MiniGameCollection::ColorTerritory {

struct TileCoord {
    int x = 0;
    int y = 0;

    bool operator==(const TileCoord&) const = default;
};

inline constexpr std::int16_t UnclaimedOwner = -1;

struct PaintResult {
    bool changed = false;
    std::int16_t previousOwner = UnclaimedOwner;
    PlayerId newOwner = InvalidPlayerId;
};

class TerritoryBoard {
public:
    TerritoryBoard(int width, int height, std::size_t playerCount)
        : m_width(width),
          m_height(height),
          m_ownerByTile(CheckedTileCount(width, height), UnclaimedOwner),
          m_scores(playerCount, 0) {
        if (playerCount == 0 || playerCount > InvalidPlayerId) {
            throw std::invalid_argument("TerritoryBoard requires 1..254 players");
        }
    }

    int GetWidth() const noexcept { return m_width; }
    int GetHeight() const noexcept { return m_height; }
    std::size_t GetTileCount() const noexcept { return m_ownerByTile.size(); }
    std::size_t GetPlayerCount() const noexcept { return m_scores.size(); }

    bool IsInside(TileCoord coord) const noexcept {
        return coord.x >= 0 && coord.y >= 0 &&
            coord.x < m_width && coord.y < m_height;
    }

    std::size_t ToIndex(TileCoord coord) const {
        if (!IsInside(coord)) {
            throw std::out_of_range("TerritoryBoard coordinate is outside the board");
        }
        return static_cast<std::size_t>(coord.y * m_width + coord.x);
    }

    TileCoord ToCoord(std::size_t index) const {
        if (index >= m_ownerByTile.size()) {
            throw std::out_of_range("TerritoryBoard index is outside the board");
        }
        return {
            static_cast<int>(index % static_cast<std::size_t>(m_width)),
            static_cast<int>(index / static_cast<std::size_t>(m_width))
        };
    }

    std::int16_t GetOwner(TileCoord coord) const {
        return m_ownerByTile[ToIndex(coord)];
    }

    int GetScore(PlayerId playerId) const {
        ValidatePlayer(playerId);
        return m_scores[playerId];
    }

    const std::vector<int>& GetScores() const noexcept {
        return m_scores;
    }

    int CountUnclaimed() const noexcept {
        return static_cast<int>(std::count(
            m_ownerByTile.begin(),
            m_ownerByTile.end(),
            UnclaimedOwner
        ));
    }

    PaintResult Paint(TileCoord coord, PlayerId playerId) {
        ValidatePlayer(playerId);
        const std::size_t index = ToIndex(coord);
        const std::int16_t previous = m_ownerByTile[index];
        if (previous == static_cast<std::int16_t>(playerId)) {
            return {
                .changed = false,
                .previousOwner = previous,
                .newOwner = playerId
            };
        }

        if (previous != UnclaimedOwner) {
            --m_scores[static_cast<std::size_t>(previous)];
        }
        m_ownerByTile[index] = static_cast<std::int16_t>(playerId);
        ++m_scores[playerId];

        return {
            .changed = true,
            .previousOwner = previous,
            .newOwner = playerId
        };
    }

    PaintResult Clear(TileCoord coord) {
        const std::size_t index = ToIndex(coord);
        const std::int16_t previous = m_ownerByTile[index];
        if (previous == UnclaimedOwner) {
            return {
                .changed = false,
                .previousOwner = previous,
                .newOwner = InvalidPlayerId
            };
        }

        --m_scores[static_cast<std::size_t>(previous)];
        m_ownerByTile[index] = UnclaimedOwner;
        return {
            .changed = true,
            .previousOwner = previous,
            .newOwner = InvalidPlayerId
        };
    }

    PlayerId FindLeader() const noexcept {
        if (m_scores.empty()) {
            return InvalidPlayerId;
        }
        const int bestScore = *std::max_element(m_scores.begin(), m_scores.end());
        const std::size_t leaderCount = static_cast<std::size_t>(std::count(
            m_scores.begin(),
            m_scores.end(),
            bestScore
        ));
        if (leaderCount != 1) {
            return InvalidPlayerId;
        }
        return static_cast<PlayerId>(std::distance(
            m_scores.begin(),
            std::find(m_scores.begin(), m_scores.end(), bestScore)
        ));
    }

    int CountAdjacentOwned(TileCoord coord, PlayerId playerId) const noexcept {
        return CountAdjacentMatching(coord, [playerId](std::int16_t owner) {
            return owner == static_cast<std::int16_t>(playerId);
        });
    }

    int CountAdjacentUnclaimed(TileCoord coord) const noexcept {
        return CountAdjacentMatching(coord, [](std::int16_t owner) {
            return owner == UnclaimedOwner;
        });
    }

    int CountAdjacentEnemy(TileCoord coord, PlayerId playerId) const noexcept {
        return CountAdjacentMatching(coord, [playerId](std::int16_t owner) {
            return owner != UnclaimedOwner &&
                owner != static_cast<std::int16_t>(playerId);
        });
    }

private:
    template<typename Predicate>
    int CountAdjacentMatching(TileCoord coord, Predicate predicate) const noexcept {
        static constexpr TileCoord offsets[] = {
            {-1, 0}, {1, 0}, {0, -1}, {0, 1}
        };

        int count = 0;
        for (const TileCoord offset : offsets) {
            const TileCoord neighbor{coord.x + offset.x, coord.y + offset.y};
            if (!IsInside(neighbor)) {
                continue;
            }
            const std::size_t index = static_cast<std::size_t>(
                neighbor.y * m_width + neighbor.x
            );
            if (predicate(m_ownerByTile[index])) {
                ++count;
            }
        }
        return count;
    }

    static std::size_t CheckedTileCount(int width, int height) {
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument("TerritoryBoard dimensions must be positive");
        }
        return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }

    void ValidatePlayer(PlayerId playerId) const {
        if (playerId >= m_scores.size()) {
            throw std::out_of_range("TerritoryBoard player id is invalid");
        }
    }

    int m_width = 0;
    int m_height = 0;
    std::vector<std::int16_t> m_ownerByTile;
    std::vector<int> m_scores;
};

struct CpuTargetContext {
    PlayerId self = InvalidPlayerId;
    TileCoord currentTile{};
    float remainingTimeRatio = 1.0f;
    std::vector<std::uint8_t> crowdByTile;
};

struct CpuTargetDecision {
    TileCoord target{};
    float utility = -std::numeric_limits<float>::infinity();
    bool attacksLeader = false;
};

class TerritoryCpuEvaluator {
public:
    static std::optional<CpuTargetDecision> ChooseTarget(
        const TerritoryBoard& board,
        const CpuTargetContext& context,
        const CpuDifficultyProfile& difficulty
    ) {
        if (context.self == InvalidPlayerId ||
            context.self >= board.GetPlayerCount() ||
            !board.IsInside(context.currentTile)) {
            return std::nullopt;
        }

        const PlayerId leader = board.FindLeader();
        const float remaining = std::clamp(context.remainingTimeRatio, 0.0f, 1.0f);
        const float endgame = 1.0f - remaining;
        const float radius = std::max(1.0f, difficulty.informationRadius);
        const float radiusSquared = radius * radius;

        std::optional<CpuTargetDecision> best;
        for (std::size_t index = 0; index < board.GetTileCount(); ++index) {
            const TileCoord candidate = board.ToCoord(index);
            const float dx = static_cast<float>(candidate.x - context.currentTile.x);
            const float dy = static_cast<float>(candidate.y - context.currentTile.y);
            const float distanceSquared = dx * dx + dy * dy;
            if (distanceSquared > radiusSquared) {
                continue;
            }

            const float distance = std::sqrt(distanceSquared);
            const std::int16_t owner = board.GetOwner(candidate);
            const bool isSelfTile =
                owner == static_cast<std::int16_t>(context.self);
            const bool isLeaderTile =
                leader != InvalidPlayerId &&
                leader != context.self &&
                owner == static_cast<std::int16_t>(leader);
            const int adjacentUnclaimed = board.CountAdjacentUnclaimed(candidate);
            const int adjacentEnemy = board.CountAdjacentEnemy(candidate, context.self);
            const int adjacentOpportunity = adjacentUnclaimed + adjacentEnemy;

            float utility = 0.0f;
            if (owner == UnclaimedOwner) {
                utility += 4.2f;
            } else if (isSelfTile) {
                // 自陣の奥へ戻る行動には価値を与えない。
                // 次の空地・敵地へ接続する境界だけを経由地点として評価する。
                utility -= 1.7f;
                utility += static_cast<float>(adjacentOpportunity) * 1.05f;
                utility -= static_cast<float>(
                    board.CountAdjacentOwned(candidate, context.self)
                ) * 0.18f;
            } else {
                // 敵地を奪うと自分+1/相手-1で点差が2動くため、空地より高く評価する。
                utility += 5.0f;
                utility += static_cast<float>(adjacentEnemy) * 0.28f;
                if (isLeaderTile) {
                    utility += 1.4f +
                        endgame * 4.2f * difficulty.lateGameAggression;
                }
            }

            utility += static_cast<float>(adjacentOpportunity) * 0.32f;
            utility -= distance * (isSelfTile ? 0.22f : 0.34f);

            if (index < context.crowdByTile.size()) {
                utility -= static_cast<float>(context.crowdByTile[index]) * 1.35f;
            }

            if (candidate == context.currentTile) {
                utility -= 2.2f;
            }

            CpuTargetDecision decision{
                .target = candidate,
                .utility = utility,
                .attacksLeader = isLeaderTile
            };

            if (!best || IsBetter(decision, *best, context.currentTile, board)) {
                best = decision;
            }
        }

        return best;
    }

private:
    static bool IsBetter(
        const CpuTargetDecision& candidate,
        const CpuTargetDecision& currentBest,
        TileCoord origin,
        const TerritoryBoard& board
    ) {
        constexpr float epsilon = 0.0001f;
        if (candidate.utility > currentBest.utility + epsilon) {
            return true;
        }
        if (candidate.utility + epsilon < currentBest.utility) {
            return false;
        }

        const auto distanceSquared = [origin](TileCoord coord) {
            const int dx = coord.x - origin.x;
            const int dy = coord.y - origin.y;
            return dx * dx + dy * dy;
        };

        const int candidateDistance = distanceSquared(candidate.target);
        const int currentDistance = distanceSquared(currentBest.target);
        if (candidateDistance != currentDistance) {
            return candidateDistance < currentDistance;
        }

        return board.ToIndex(candidate.target) < board.ToIndex(currentBest.target);
    }
};

} // namespace MiniGameCollection::ColorTerritory
