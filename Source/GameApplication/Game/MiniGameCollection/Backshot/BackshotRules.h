#pragma once

#include "Game/MiniGameCollection/Backshot/BackshotModel.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

namespace MiniGameCollection::Backshot {

struct PendingShot {
    PlayerId attacker = InvalidPlayerId;
    PlayerId intendedTarget = InvalidPlayerId;
    bool hasLineOfSight = true;
};

struct BackshotEvent {
    ShotResult shot{};
    bool eliminatedVictim = false;
    std::size_t livingPlayerCount = 0;
};

class BackshotRules final : public IMiniGameRules {
public:
    BackshotRules(
        std::size_t playerCount = 4,
        float durationSeconds = 35.0f,
        BackshotConfig config = {}
    )
        : m_playerCount(playerCount),
          m_durationSeconds(std::max(1.0f, durationSeconds)),
          m_config(config) {
        if (playerCount < 2 || playerCount > InvalidPlayerId) {
            throw std::invalid_argument("BackshotRules requires 2..254 players");
        }
    }

    void Prepare() override {
        m_combatants.assign(m_playerCount, {});
        m_eliminations.assign(m_playerCount, 0);
        m_eliminationTime.assign(m_playerCount, m_durationSeconds);
        for (std::size_t index = 0; index < m_playerCount; ++index) {
            CombatantSnapshot& combatant = m_combatants[index];
            combatant.playerId = static_cast<PlayerId>(index);
            combatant.alive = true;
            combatant.forward = {0.0f, 1.0f};
        }
        m_pendingShots.clear();
        m_events.clear();
        m_elapsedSeconds = 0.0f;
        m_started = false;
        m_finished = false;
    }

    void StartGame() override {
        if (m_combatants.size() != m_playerCount) {
            throw std::logic_error("BackshotRules is not prepared");
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

        for (CombatantSnapshot& combatant : m_combatants) {
            combatant.shotCooldownRemainingSeconds = std::max(
                0.0f,
                combatant.shotCooldownRemainingSeconds - delta
            );
        }

        for (const PendingShot& pending : m_pendingShots) {
            ProcessShot(pending);
            if (CountLivingPlayers() <= 1) {
                break;
            }
        }
        m_pendingShots.clear();

        if (CountLivingPlayers() <= 1 || m_elapsedSeconds >= m_durationSeconds) {
            m_finished = true;
        }
    }

    bool IsFinished() const override {
        return m_finished;
    }

    MiniGameResult BuildResult() const override {
        MiniGameResult result;
        result.gameId = MiniGameId::Backshot;
        result.players.reserve(m_playerCount);

        for (std::size_t index = 0; index < m_playerCount; ++index) {
            const CombatantSnapshot& combatant = m_combatants[index];
            result.players.push_back({
                .playerId = combatant.playerId,
                .score = m_eliminations[index],
                .eliminated = !combatant.alive,
                .finishTimeSeconds = combatant.alive
                    ? m_elapsedSeconds
                    : m_eliminationTime[index]
            });
        }
        result.RebuildRanking();
        return result;
    }

    void Shutdown() override {
        m_started = false;
        m_finished = true;
        m_combatants.clear();
        m_eliminations.clear();
        m_eliminationTime.clear();
        m_pendingShots.clear();
        m_events.clear();
    }

    bool UpdateCombatant(
        PlayerId playerId,
        Vec2 position,
        Vec2 forward
    ) noexcept {
        if (playerId >= m_combatants.size()) {
            return false;
        }
        CombatantSnapshot& combatant = m_combatants[playerId];
        combatant.position = position;
        const Vec2 normalizedForward = NormalizeOrZero(forward);
        if (LengthSquared(normalizedForward) > 0.0001f) {
            combatant.forward = normalizedForward;
        }
        return true;
    }

    bool QueueShot(
        PlayerId attacker,
        PlayerId intendedTarget,
        bool hasLineOfSight
    ) {
        if (!m_started || m_finished ||
            attacker >= m_combatants.size() ||
            intendedTarget >= m_combatants.size() ||
            attacker == intendedTarget ||
            !m_combatants[attacker].alive) {
            return false;
        }

        const auto duplicate = std::find_if(
            m_pendingShots.begin(),
            m_pendingShots.end(),
            [attacker](const PendingShot& shot) {
                return shot.attacker == attacker;
            }
        );
        if (duplicate != m_pendingShots.end()) {
            return false;
        }

        m_pendingShots.push_back({
            .attacker = attacker,
            .intendedTarget = intendedTarget,
            .hasLineOfSight = hasLineOfSight
        });
        return true;
    }

    std::vector<BackshotEvent> ConsumeEvents() {
        std::vector<BackshotEvent> events;
        events.swap(m_events);
        return events;
    }

    const std::vector<CombatantSnapshot>& GetCombatants() const noexcept {
        return m_combatants;
    }
    const std::vector<int>& GetEliminations() const noexcept {
        return m_eliminations;
    }
    std::size_t GetLivingPlayerCount() const noexcept {
        return CountLivingPlayers();
    }
    float GetElapsedSeconds() const noexcept { return m_elapsedSeconds; }
    float GetRemainingSeconds() const noexcept {
        return std::max(0.0f, m_durationSeconds - m_elapsedSeconds);
    }

private:
    void ProcessShot(const PendingShot& pending) {
        CombatantSnapshot& attacker = m_combatants[pending.attacker];
        CombatantSnapshot& victim = m_combatants[pending.intendedTarget];

        ShotResult shot = BackshotHitResolver::Resolve(
            attacker,
            victim,
            pending.hasLineOfSight,
            m_config
        );

        if (shot.resolution != ShotResolution::Cooldown && attacker.alive) {
            attacker.shotCooldownRemainingSeconds = std::max(
                0.0f,
                m_config.cooldownSeconds
            );
        }

        bool eliminatedVictim = false;
        if (shot.resolution == ShotResolution::RearElimination && victim.alive) {
            victim.alive = false;
            m_eliminationTime[victim.playerId] = m_elapsedSeconds;
            ++m_eliminations[attacker.playerId];
            eliminatedVictim = true;
        }

        m_events.push_back({
            .shot = shot,
            .eliminatedVictim = eliminatedVictim,
            .livingPlayerCount = CountLivingPlayers()
        });
    }

    std::size_t CountLivingPlayers() const noexcept {
        return static_cast<std::size_t>(std::count_if(
            m_combatants.begin(),
            m_combatants.end(),
            [](const CombatantSnapshot& combatant) {
                return combatant.alive;
            }
        ));
    }

    std::size_t m_playerCount = 0;
    float m_durationSeconds = 35.0f;
    BackshotConfig m_config{};
    std::vector<CombatantSnapshot> m_combatants;
    std::vector<int> m_eliminations;
    std::vector<float> m_eliminationTime;
    std::vector<PendingShot> m_pendingShots;
    std::vector<BackshotEvent> m_events;
    float m_elapsedSeconds = 0.0f;
    bool m_started = false;
    bool m_finished = false;
};

} // namespace MiniGameCollection::Backshot
