#pragma once

#include "Game/MiniGameCollection/SheepRoundup/SheepSteeringModel.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace MiniGameCollection::SheepRoundup {

struct SheepState {
    std::size_t sheepId = 0;
    Vec2 position{};
    Vec2 direction{0.0f, 1.0f};
    Vec2 velocity{};
    PlayerId scoredBy = InvalidPlayerId;

    bool IsScored() const noexcept {
        return scoredBy != InvalidPlayerId;
    }
};

struct SheepPenDefinition {
    PlayerId owner = InvalidPlayerId;
    Vec2 center{};
    float radius = 1.8f;
};

struct SheepScoreEvent {
    std::size_t sheepId = 0;
    PlayerId playerId = InvalidPlayerId;
    int newScore = 0;
    bool changedLeader = false;
};

class SheepRoundupRules final : public IMiniGameRules {
public:
    SheepRoundupRules(
        std::size_t playerCount = 4,
        float durationSeconds = 50.0f,
        Bounds2 movementBounds = {{-10.0f, -7.0f}, {10.0f, 7.0f}}
    )
        : m_playerCount(playerCount),
          m_durationSeconds(std::max(1.0f, durationSeconds)),
          m_bounds(movementBounds) {
        if (playerCount == 0 || playerCount > InvalidPlayerId) {
            throw std::invalid_argument("SheepRoundupRules requires 1..254 players");
        }
    }

    void SetInitialSheep(std::vector<Vec2> positions) {
        if (m_started) {
            throw std::logic_error("Cannot change sheep layout while playing");
        }
        m_initialSheepPositions = std::move(positions);
    }

    void SetPens(std::vector<SheepPenDefinition> pens) {
        if (m_started) {
            throw std::logic_error("Cannot change pens while playing");
        }
        m_penDefinitions = std::move(pens);
    }

    void SetSteeringConfig(SheepSteeringConfig config) noexcept {
        m_steeringConfig = config;
    }

    void Prepare() override {
        EnsureDefaultLayout();
        ValidatePens();

        m_sheep.clear();
        m_sheep.reserve(m_initialSheepPositions.size());
        for (std::size_t index = 0; index < m_initialSheepPositions.size(); ++index) {
            m_sheep.push_back({
                .sheepId = index,
                .position = SheepSteeringModel::ClampInsideBounds(
                    m_initialSheepPositions[index],
                    m_bounds,
                    0.3f
                )
            });
        }

        m_playerPositions.assign(m_playerCount, {});
        m_scores.assign(m_playerCount, 0);
        m_events.clear();
        m_elapsedSeconds = 0.0f;
        m_started = false;
        m_finished = false;
        m_lastLeader = InvalidPlayerId;
    }

    void StartGame() override {
        if (m_sheep.empty()) {
            throw std::logic_error("SheepRoundupRules has no sheep");
        }
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

        std::vector<Vec2> flockPositions;
        flockPositions.reserve(m_sheep.size());
        for (const SheepState& sheep : m_sheep) {
            if (!sheep.IsScored()) {
                flockPositions.push_back(sheep.position);
            }
        }

        for (SheepState& sheep : m_sheep) {
            if (sheep.IsScored()) {
                sheep.velocity = {};
                continue;
            }

            SheepSteeringInput input;
            input.position = sheep.position;
            input.previousDirection = sheep.direction;
            input.playerPositions = m_playerPositions;
            input.flockPositions = flockPositions;
            input.movementBounds = m_bounds;

            const SheepSteeringOutput steering = SheepSteeringModel::Compute(
                input,
                m_steeringConfig,
                delta
            );
            sheep.direction = steering.direction;
            sheep.velocity = steering.velocity;
            sheep.position += sheep.velocity * delta;
            sheep.position = SheepSteeringModel::ClampInsideBounds(
                sheep.position,
                m_bounds,
                0.25f
            );

            TryScoreSheep(sheep);
        }

        if (m_elapsedSeconds >= m_durationSeconds ||
            std::all_of(
                m_sheep.begin(),
                m_sheep.end(),
                [](const SheepState& sheep) { return sheep.IsScored(); }
            )) {
            m_finished = true;
        }
    }

    bool IsFinished() const override {
        return m_finished;
    }

    MiniGameResult BuildResult() const override {
        MiniGameResult result;
        result.gameId = MiniGameId::SheepRoundup;
        result.players.reserve(m_playerCount);
        for (std::size_t index = 0; index < m_playerCount; ++index) {
            result.players.push_back({
                .playerId = static_cast<PlayerId>(index),
                .score = m_scores[index],
                .eliminated = false,
                .finishTimeSeconds = m_elapsedSeconds
            });
        }
        result.RebuildRanking();
        return result;
    }

    void Shutdown() override {
        m_started = false;
        m_finished = true;
        m_sheep.clear();
        m_playerPositions.clear();
        m_scores.clear();
        m_events.clear();
    }

    bool SetPlayerPosition(PlayerId playerId, Vec2 position) noexcept {
        if (playerId >= m_playerPositions.size()) {
            return false;
        }
        m_playerPositions[playerId] = SheepSteeringModel::ClampInsideBounds(
            position,
            m_bounds,
            0.25f
        );
        return true;
    }

    std::vector<SheepScoreEvent> ConsumeScoreEvents() {
        std::vector<SheepScoreEvent> events;
        events.swap(m_events);
        return events;
    }

    const std::vector<SheepState>& GetSheep() const noexcept { return m_sheep; }
    const std::vector<int>& GetScores() const noexcept { return m_scores; }
    const std::vector<SheepPenDefinition>& GetPens() const noexcept {
        return m_penDefinitions;
    }
    float GetElapsedSeconds() const noexcept { return m_elapsedSeconds; }
    float GetRemainingSeconds() const noexcept {
        return std::max(0.0f, m_durationSeconds - m_elapsedSeconds);
    }

private:
    void EnsureDefaultLayout() {
        if (m_initialSheepPositions.empty()) {
            m_initialSheepPositions = {
                {-2.0f, -1.5f}, {0.0f, -1.8f}, {2.0f, -1.4f},
                {-2.3f, 1.3f}, {0.0f, 1.7f}, {2.2f, 1.2f},
                {-0.8f, 0.0f}, {0.9f, 0.1f}
            };
        }

        if (!m_penDefinitions.empty()) {
            return;
        }

        const Vec2 center{
            (m_bounds.minimum.x + m_bounds.maximum.x) * 0.5f,
            (m_bounds.minimum.y + m_bounds.maximum.y) * 0.5f
        };
        const float sidePadding = 1.6f;
        const Vec2 candidates[] = {
            {center.x, m_bounds.minimum.y + sidePadding},
            {m_bounds.maximum.x - sidePadding, center.y},
            {center.x, m_bounds.maximum.y - sidePadding},
            {m_bounds.minimum.x + sidePadding, center.y}
        };
        for (std::size_t index = 0; index < m_playerCount; ++index) {
            m_penDefinitions.push_back({
                .owner = static_cast<PlayerId>(index),
                .center = candidates[index % 4],
                .radius = 1.6f
            });
        }
    }

    void ValidatePens() const {
        std::vector<bool> hasPen(m_playerCount, false);
        for (const SheepPenDefinition& pen : m_penDefinitions) {
            if (pen.owner >= m_playerCount || pen.radius <= 0.0f) {
                throw std::invalid_argument("Sheep pen definition is invalid");
            }
            if (hasPen[pen.owner]) {
                throw std::invalid_argument("Each player must have exactly one sheep pen");
            }
            hasPen[pen.owner] = true;
        }
        if (std::find(hasPen.begin(), hasPen.end(), false) != hasPen.end()) {
            throw std::invalid_argument("A player is missing a sheep pen");
        }
    }

    void TryScoreSheep(SheepState& sheep) {
        for (const SheepPenDefinition& pen : m_penDefinitions) {
            if (DistanceSquared(sheep.position, pen.center) > pen.radius * pen.radius) {
                continue;
            }

            const PlayerId previousLeader = FindLeader();
            sheep.scoredBy = pen.owner;
            sheep.position = pen.center;
            sheep.velocity = {};
            ++m_scores[pen.owner];
            const PlayerId newLeader = FindLeader();
            m_events.push_back({
                .sheepId = sheep.sheepId,
                .playerId = pen.owner,
                .newScore = m_scores[pen.owner],
                .changedLeader =
                    previousLeader != newLeader &&
                    newLeader != InvalidPlayerId
            });
            m_lastLeader = newLeader;
            return;
        }
    }

    PlayerId FindLeader() const noexcept {
        if (m_scores.empty()) {
            return InvalidPlayerId;
        }
        return static_cast<PlayerId>(std::distance(
            m_scores.begin(),
            std::max_element(m_scores.begin(), m_scores.end())
        ));
    }

    std::size_t m_playerCount = 0;
    float m_durationSeconds = 50.0f;
    Bounds2 m_bounds{};
    SheepSteeringConfig m_steeringConfig{};
    std::vector<Vec2> m_initialSheepPositions;
    std::vector<SheepPenDefinition> m_penDefinitions;
    std::vector<SheepState> m_sheep;
    std::vector<Vec2> m_playerPositions;
    std::vector<int> m_scores;
    std::vector<SheepScoreEvent> m_events;
    float m_elapsedSeconds = 0.0f;
    bool m_started = false;
    bool m_finished = false;
    PlayerId m_lastLeader = InvalidPlayerId;
};

} // namespace MiniGameCollection::SheepRoundup
