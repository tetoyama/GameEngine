#pragma once

#include "Game/MiniGameCollection/SheepRoundup/SheepSteeringModel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace MiniGameCollection::SheepRoundup {

struct SheepSpawnDefinition {
    Vec2 position{};
    bool golden = false;
};

struct SheepSpawnConfig {
    std::size_t poolCapacity = 24;
    std::size_t earlyTargetActive = 8;
    std::size_t lateTargetActive = 18;
    float latePhaseStartRemainingRatio = 0.5f;
    float earlySpawnIntervalSeconds = 2.4f;
    float lateSpawnIntervalSeconds = 0.72f;
    std::size_t earlySpawnBatch = 1;
    std::size_t lateSpawnBatch = 2;
    float earlyGoldenChance = 0.10f;
    float lateGoldenChance = 0.26f;
    int normalScoreValue = 1;
    int goldenScoreValue = 3;
    float spawnPlayerClearance = 1.4f;
    float spawnSheepClearance = 0.85f;
    float spawnPenClearance = 1.35f;
};

struct SheepState {
    std::size_t sheepId = 0;
    Vec2 position{};
    Vec2 direction{0.0f, 1.0f};
    Vec2 velocity{};
    PlayerId scoredBy = InvalidPlayerId;
    int scoreValue = 1;
    std::uint32_t generation = 0;
    bool active = false;
    bool golden = false;

    bool IsActive() const noexcept {
        return active;
    }

    bool IsScored() const noexcept {
        return !active && scoredBy != InvalidPlayerId;
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
    Vec2 position{};
    int pointsAwarded = 1;
    int newScore = 0;
    bool golden = false;
    bool changedLeader = false;
};

struct SheepSpawnEvent {
    std::size_t sheepId = 0;
    Vec2 position{};
    int scoreValue = 1;
    std::uint32_t generation = 0;
    bool golden = false;
    bool lateRush = false;
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
        m_initialSheep.clear();
        m_initialSheep.reserve(positions.size());
        for (const Vec2 position : positions) {
            m_initialSheep.push_back({.position = position, .golden = false});
        }
    }

    void SetInitialSheepDefinitions(std::vector<SheepSpawnDefinition> sheep) {
        if (m_started) {
            throw std::logic_error("Cannot change sheep layout while playing");
        }
        m_initialSheep = std::move(sheep);
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

    void SetSpawnConfig(SheepSpawnConfig config) {
        if (m_started) {
            throw std::logic_error("Cannot change sheep spawn config while playing");
        }
        if (config.poolCapacity == 0 ||
            config.normalScoreValue <= 0 ||
            config.goldenScoreValue <= 0 ||
            config.earlySpawnIntervalSeconds <= 0.0f ||
            config.lateSpawnIntervalSeconds <= 0.0f) {
            throw std::invalid_argument("Sheep spawn config is invalid");
        }
        m_spawnConfig = config;
    }

    void SetSpawnSeed(std::uint32_t seed) noexcept {
        m_spawnSeed = seed != 0 ? seed : DefaultSpawnSeed;
    }

    void Prepare() override {
        EnsureDefaultLayout();
        ValidatePens();

        const std::size_t capacity = std::max(
            m_spawnConfig.poolCapacity,
            m_initialSheep.size()
        );
        m_sheep.assign(capacity, {});
        for (std::size_t index = 0; index < m_sheep.size(); ++index) {
            m_sheep[index].sheepId = index;
        }

        for (std::size_t index = 0; index < m_initialSheep.size(); ++index) {
            ActivateSheep(
                m_sheep[index],
                SheepSteeringModel::ClampInsideBounds(
                    m_initialSheep[index].position,
                    m_bounds,
                    0.3f
                ),
                m_initialSheep[index].golden,
                false
            );
        }

        m_playerPositions.assign(m_playerCount, {});
        m_scores.assign(m_playerCount, 0);
        m_scoreEvents.clear();
        m_spawnEvents.clear();
        m_elapsedSeconds = 0.0f;
        m_nextSpawnSeconds = m_spawnConfig.earlySpawnIntervalSeconds;
        m_started = false;
        m_finished = false;
        m_lastLeader = InvalidPlayerId;
        m_spawnRandomState = m_spawnSeed;
        m_normalSpawnsSinceGolden = 0;
    }

    void StartGame() override {
        if (GetActiveSheepCount() == 0) {
            SpawnUntilTarget(false);
        }
        if (GetActiveSheepCount() == 0) {
            throw std::logic_error("SheepRoundupRules could not spawn sheep");
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
        flockPositions.reserve(GetActiveSheepCount());
        for (const SheepState& sheep : m_sheep) {
            if (sheep.IsActive()) {
                flockPositions.push_back(sheep.position);
            }
        }

        for (SheepState& sheep : m_sheep) {
            if (!sheep.IsActive()) {
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

        UpdateSpawning(delta);

        if (m_elapsedSeconds >= m_durationSeconds) {
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
        m_scoreEvents.clear();
        m_spawnEvents.clear();
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
        events.swap(m_scoreEvents);
        return events;
    }

    std::vector<SheepSpawnEvent> ConsumeSpawnEvents() {
        std::vector<SheepSpawnEvent> events;
        events.swap(m_spawnEvents);
        return events;
    }

    const std::vector<SheepState>& GetSheep() const noexcept { return m_sheep; }
    const std::vector<int>& GetScores() const noexcept { return m_scores; }
    const std::vector<SheepPenDefinition>& GetPens() const noexcept {
        return m_penDefinitions;
    }
    const SheepSpawnConfig& GetSpawnConfig() const noexcept {
        return m_spawnConfig;
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
    bool IsLateRush() const noexcept {
        return GetRemainingTimeRatio() <=
            std::clamp(m_spawnConfig.latePhaseStartRemainingRatio, 0.0f, 1.0f);
    }
    std::size_t GetActiveSheepCount() const noexcept {
        return static_cast<std::size_t>(std::count_if(
            m_sheep.begin(),
            m_sheep.end(),
            [](const SheepState& sheep) { return sheep.IsActive(); }
        ));
    }
    std::size_t GetActiveGoldenSheepCount() const noexcept {
        return static_cast<std::size_t>(std::count_if(
            m_sheep.begin(),
            m_sheep.end(),
            [](const SheepState& sheep) {
                return sheep.IsActive() && sheep.golden;
            }
        ));
    }

private:
    static constexpr std::uint32_t DefaultSpawnSeed = 0x5EE9A11u;

    void EnsureDefaultLayout() {
        if (m_initialSheep.empty()) {
            const Vec2 defaults[] = {
                {-2.0f, -1.5f}, {0.0f, -1.8f}, {2.0f, -1.4f},
                {-2.3f, 1.3f}, {0.0f, 1.7f}, {2.2f, 1.2f},
                {-0.8f, 0.0f}, {0.9f, 0.1f}
            };
            for (const Vec2 position : defaults) {
                m_initialSheep.push_back({
                    .position = position,
                    .golden = false
                });
            }
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
            if (DistanceSquared(sheep.position, pen.center) >
                pen.radius * pen.radius) {
                continue;
            }

            const PlayerId previousLeader = FindLeader();
            const int points = std::max(1, sheep.scoreValue);
            const bool wasGolden = sheep.golden;
            const Vec2 scorePosition = pen.center;

            sheep.active = false;
            sheep.scoredBy = pen.owner;
            sheep.position = scorePosition;
            sheep.velocity = {};
            m_scores[pen.owner] += points;

            const PlayerId newLeader = FindLeader();
            m_scoreEvents.push_back({
                .sheepId = sheep.sheepId,
                .playerId = pen.owner,
                .position = scorePosition,
                .pointsAwarded = points,
                .newScore = m_scores[pen.owner],
                .golden = wasGolden,
                .changedLeader =
                    previousLeader != newLeader &&
                    newLeader != InvalidPlayerId
            });
            m_lastLeader = newLeader;
            return;
        }
    }

    void UpdateSpawning(float deltaTime) {
        const bool lateRush = IsLateRush();
        m_nextSpawnSeconds -= deltaTime;
        if (m_nextSpawnSeconds > 0.0f) {
            return;
        }

        const std::size_t target = std::min(
            m_sheep.size(),
            lateRush
                ? m_spawnConfig.lateTargetActive
                : m_spawnConfig.earlyTargetActive
        );
        const std::size_t batch = std::max<std::size_t>(
            1,
            lateRush
                ? m_spawnConfig.lateSpawnBatch
                : m_spawnConfig.earlySpawnBatch
        );

        std::size_t spawned = 0;
        while (spawned < batch && GetActiveSheepCount() < target) {
            if (!SpawnOne(lateRush)) {
                break;
            }
            ++spawned;
        }

        const float baseInterval = lateRush
            ? m_spawnConfig.lateSpawnIntervalSeconds
            : m_spawnConfig.earlySpawnIntervalSeconds;
        const float jitter = 0.82f + NextUnit() * 0.36f;
        m_nextSpawnSeconds = std::max(0.1f, baseInterval * jitter);

        if (GetActiveSheepCount() + 2 < target) {
            m_nextSpawnSeconds = std::min(m_nextSpawnSeconds, 0.25f);
        }
    }

    void SpawnUntilTarget(bool emitEvents) {
        const bool lateRush = IsLateRush();
        const std::size_t target = std::min(
            m_sheep.size(),
            lateRush
                ? m_spawnConfig.lateTargetActive
                : m_spawnConfig.earlyTargetActive
        );
        while (GetActiveSheepCount() < target) {
            if (!SpawnOne(lateRush, emitEvents)) {
                break;
            }
        }
    }

    bool SpawnOne(bool lateRush, bool emitEvent = true) {
        auto available = std::find_if(
            m_sheep.begin(),
            m_sheep.end(),
            [](const SheepState& sheep) { return !sheep.IsActive(); }
        );
        if (available == m_sheep.end()) {
            return false;
        }

        const bool golden = RollGolden(lateRush);
        const Vec2 position = ChooseSpawnPosition();
        ActivateSheep(*available, position, golden, emitEvent);
        return true;
    }

    void ActivateSheep(
        SheepState& sheep,
        Vec2 position,
        bool golden,
        bool emitEvent
    ) {
        sheep.position = position;
        sheep.direction = RandomDirection();
        sheep.velocity = {};
        sheep.scoredBy = InvalidPlayerId;
        sheep.golden = golden;
        sheep.scoreValue = golden
            ? std::max(1, m_spawnConfig.goldenScoreValue)
            : std::max(1, m_spawnConfig.normalScoreValue);
        sheep.active = true;
        ++sheep.generation;

        if (emitEvent) {
            m_spawnEvents.push_back({
                .sheepId = sheep.sheepId,
                .position = sheep.position,
                .scoreValue = sheep.scoreValue,
                .generation = sheep.generation,
                .golden = sheep.golden,
                .lateRush = IsLateRush()
            });
        }
    }

    bool RollGolden(bool lateRush) {
        const std::size_t activeGolden = GetActiveGoldenSheepCount();
        const std::size_t goldenCap = lateRush ? 3 : 1;
        if (activeGolden >= goldenCap) {
            ++m_normalSpawnsSinceGolden;
            return false;
        }

        const int pityThreshold = lateRush ? 4 : 7;
        const float chance = std::clamp(
            lateRush
                ? m_spawnConfig.lateGoldenChance
                : m_spawnConfig.earlyGoldenChance,
            0.0f,
            1.0f
        );
        const bool golden =
            m_normalSpawnsSinceGolden >= pityThreshold ||
            NextUnit() < chance;
        if (golden) {
            m_normalSpawnsSinceGolden = 0;
        } else {
            ++m_normalSpawnsSinceGolden;
        }
        return golden;
    }

    Vec2 ChooseSpawnPosition() {
        const Vec2 center{
            (m_bounds.minimum.x + m_bounds.maximum.x) * 0.5f,
            (m_bounds.minimum.y + m_bounds.maximum.y) * 0.5f
        };
        const float halfWidth =
            (m_bounds.maximum.x - m_bounds.minimum.x) * 0.34f;
        const float halfHeight =
            (m_bounds.maximum.y - m_bounds.minimum.y) * 0.34f;

        for (int attempt = 0; attempt < 32; ++attempt) {
            const Vec2 candidate{
                center.x + (NextUnit() * 2.0f - 1.0f) * halfWidth,
                center.y + (NextUnit() * 2.0f - 1.0f) * halfHeight
            };
            if (IsSpawnPositionClear(candidate)) {
                return SheepSteeringModel::ClampInsideBounds(
                    candidate,
                    m_bounds,
                    0.5f
                );
            }
        }

        const float angle = NextUnit() * 6.28318530718f;
        return SheepSteeringModel::ClampInsideBounds(
            {
                center.x + std::cos(angle) * 2.2f,
                center.y + std::sin(angle) * 2.2f
            },
            m_bounds,
            0.5f
        );
    }

    bool IsSpawnPositionClear(Vec2 position) const {
        for (const SheepPenDefinition& pen : m_penDefinitions) {
            const float clearance =
                pen.radius + m_spawnConfig.spawnPenClearance;
            if (DistanceSquared(position, pen.center) <
                clearance * clearance) {
                return false;
            }
        }
        for (const Vec2 player : m_playerPositions) {
            if (DistanceSquared(position, player) <
                m_spawnConfig.spawnPlayerClearance *
                m_spawnConfig.spawnPlayerClearance) {
                return false;
            }
        }
        for (const SheepState& sheep : m_sheep) {
            if (!sheep.IsActive()) {
                continue;
            }
            if (DistanceSquared(position, sheep.position) <
                m_spawnConfig.spawnSheepClearance *
                m_spawnConfig.spawnSheepClearance) {
                return false;
            }
        }
        return true;
    }

    Vec2 RandomDirection() {
        const float angle = NextUnit() * 6.28318530718f;
        return {std::cos(angle), std::sin(angle)};
    }

    float NextUnit() noexcept {
        std::uint32_t value = m_spawnRandomState;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        m_spawnRandomState =
            value != 0 ? value : DefaultSpawnSeed;
        return static_cast<float>(m_spawnRandomState & 0x00FFFFFFu) /
            static_cast<float>(0x01000000u);
    }

    PlayerId FindLeader() const noexcept {
        if (m_scores.empty()) {
            return InvalidPlayerId;
        }
        const int maximum = *std::max_element(
            m_scores.begin(),
            m_scores.end()
        );
        PlayerId leader = InvalidPlayerId;
        int leaderCount = 0;
        for (std::size_t index = 0; index < m_scores.size(); ++index) {
            if (m_scores[index] != maximum) {
                continue;
            }
            leader = static_cast<PlayerId>(index);
            ++leaderCount;
        }
        return leaderCount == 1 ? leader : InvalidPlayerId;
    }

    std::size_t m_playerCount = 0;
    float m_durationSeconds = 50.0f;
    Bounds2 m_bounds{};
    SheepSteeringConfig m_steeringConfig{};
    SheepSpawnConfig m_spawnConfig{};
    std::vector<SheepSpawnDefinition> m_initialSheep;
    std::vector<SheepPenDefinition> m_penDefinitions;
    std::vector<SheepState> m_sheep;
    std::vector<Vec2> m_playerPositions;
    std::vector<int> m_scores;
    std::vector<SheepScoreEvent> m_scoreEvents;
    std::vector<SheepSpawnEvent> m_spawnEvents;
    float m_elapsedSeconds = 0.0f;
    float m_nextSpawnSeconds = 0.0f;
    bool m_started = false;
    bool m_finished = false;
    PlayerId m_lastLeader = InvalidPlayerId;
    std::uint32_t m_spawnSeed = DefaultSpawnSeed;
    std::uint32_t m_spawnRandomState = DefaultSpawnSeed;
    int m_normalSpawnsSinceGolden = 0;
};

} // namespace MiniGameCollection::SheepRoundup
