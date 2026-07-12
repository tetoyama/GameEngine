#pragma once

#include "Game/MiniGameCollection/ColorTerritory/ColorTerritoryModel.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace MiniGameCollection::ColorTerritory {

struct TerritoryPaintEvent {
    TileCoord tile{};
    PlayerId playerId = InvalidPlayerId;
    std::int16_t previousOwner = UnclaimedOwner;
    bool changed = false;
    bool changedLeader = false;
};

class ColorTerritoryRules final : public IMiniGameRules {
public:
    ColorTerritoryRules(
        int width = 13,
        int height = 9,
        std::size_t playerCount = 4,
        float durationSeconds = 40.0f
    )
        : m_width(width),
          m_height(height),
          m_playerCount(playerCount),
          m_durationSeconds(std::max(1.0f, durationSeconds)) {
    }

    void Prepare() override {
        m_board = std::make_unique<TerritoryBoard>(
            m_width,
            m_height,
            m_playerCount
        );
        m_currentTiles.assign(m_playerCount, std::nullopt);
        m_pendingTiles.assign(m_playerCount, std::nullopt);
        m_elapsedSeconds = 0.0f;
        m_started = false;
        m_finished = false;
        m_lastLeader = InvalidPlayerId;
        m_events.clear();
    }

    void StartGame() override {
        RequirePrepared();
        m_started = true;
    }

    void Tick(float deltaTime) override {
        if (!m_started || m_finished) {
            return;
        }

        const float delta = std::max(0.0f, deltaTime);
        m_elapsedSeconds = std::min(
            m_durationSeconds,
            m_elapsedSeconds + delta
        );

        for (std::size_t index = 0; index < m_pendingTiles.size(); ++index) {
            if (!m_pendingTiles[index]) {
                continue;
            }
            const PlayerId playerId = static_cast<PlayerId>(index);
            const TileCoord tile = *m_pendingTiles[index];
            m_pendingTiles[index].reset();

            if (!m_board->IsInside(tile)) {
                continue;
            }
            if (m_currentTiles[index] && *m_currentTiles[index] == tile) {
                continue;
            }

            const PlayerId previousLeader = m_board->FindLeader();
            const PaintResult paint = m_board->Paint(tile, playerId);
            m_currentTiles[index] = tile;
            const PlayerId newLeader = m_board->FindLeader();

            if (paint.changed) {
                m_events.push_back({
                    .tile = tile,
                    .playerId = playerId,
                    .previousOwner = paint.previousOwner,
                    .changed = true,
                    .changedLeader =
                        previousLeader != newLeader &&
                        newLeader != InvalidPlayerId
                });
            }
            m_lastLeader = newLeader;
        }

        if (m_elapsedSeconds >= m_durationSeconds) {
            m_finished = true;
        }
    }

    bool IsFinished() const override {
        return m_finished;
    }

    MiniGameResult BuildResult() const override {
        RequirePrepared();
        MiniGameResult result;
        result.gameId = MiniGameId::ColorTerritory;
        result.players.reserve(m_playerCount);
        for (std::size_t index = 0; index < m_playerCount; ++index) {
            result.players.push_back({
                .playerId = static_cast<PlayerId>(index),
                .score = m_board->GetScore(static_cast<PlayerId>(index)),
                .eliminated = false,
                .finishTimeSeconds = m_durationSeconds
            });
        }
        result.RebuildRanking();
        return result;
    }

    void Shutdown() override {
        m_started = false;
        m_finished = true;
        m_currentTiles.clear();
        m_pendingTiles.clear();
        m_events.clear();
        m_board.reset();
    }

    bool SubmitPlayerTile(PlayerId playerId, TileCoord tile) {
        if (!m_started || m_finished || playerId >= m_pendingTiles.size()) {
            return false;
        }
        if (!m_board->IsInside(tile)) {
            return false;
        }
        m_pendingTiles[playerId] = tile;
        return true;
    }

    std::vector<TerritoryPaintEvent> ConsumePaintEvents() {
        std::vector<TerritoryPaintEvent> events;
        events.swap(m_events);
        return events;
    }

    const TerritoryBoard* TryGetBoard() const noexcept {
        return m_board.get();
    }

    float GetElapsedSeconds() const noexcept { return m_elapsedSeconds; }
    float GetRemainingSeconds() const noexcept {
        return std::max(0.0f, m_durationSeconds - m_elapsedSeconds);
    }
    float GetRemainingTimeRatio() const noexcept {
        return m_durationSeconds > 0.0f
            ? GetRemainingSeconds() / m_durationSeconds
            : 0.0f;
    }
    PlayerId GetLeader() const noexcept { return m_lastLeader; }

private:
    void RequirePrepared() const {
        if (!m_board) {
            throw std::logic_error("ColorTerritoryRules is not prepared");
        }
    }

    int m_width = 0;
    int m_height = 0;
    std::size_t m_playerCount = 0;
    float m_durationSeconds = 40.0f;
    float m_elapsedSeconds = 0.0f;
    bool m_started = false;
    bool m_finished = false;
    PlayerId m_lastLeader = InvalidPlayerId;
    std::unique_ptr<TerritoryBoard> m_board;
    std::vector<std::optional<TileCoord>> m_currentTiles;
    std::vector<std::optional<TileCoord>> m_pendingTiles;
    std::vector<TerritoryPaintEvent> m_events;
};

} // namespace MiniGameCollection::ColorTerritory
