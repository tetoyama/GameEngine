#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"

#include <algorithm>
#include <cstdint>

namespace MiniGameCollection {

class DeterministicRandom {
public:
    explicit DeterministicRandom(std::uint32_t seed = 0x9E3779B9u)
        : m_state(seed != 0 ? seed : 0x9E3779B9u) {
    }

    std::uint32_t NextU32() noexcept {
        std::uint32_t value = m_state;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        m_state = value;
        return value;
    }

    float NextUnitFloat() noexcept {
        return static_cast<float>(NextU32() & 0x00FFFFFFu) /
            static_cast<float>(0x01000000u);
    }

private:
    std::uint32_t m_state;
};

class MiniGameCpuDecisionClock {
public:
    MiniGameCpuDecisionClock(
        CpuDifficultyProfile difficulty,
        std::uint32_t seed
    )
        : m_difficulty(difficulty),
          m_random(seed) {
    }

    void Reset(float initialDelaySeconds = 0.0f) noexcept {
        m_decisionRemainingSeconds = std::max(0.0f, initialDelaySeconds);
        m_targetHoldRemainingSeconds = 0.0f;
        m_hasTarget = false;
    }

    bool Tick(float deltaTime) noexcept {
        const float delta = std::max(0.0f, deltaTime);
        m_decisionRemainingSeconds =
            std::max(0.0f, m_decisionRemainingSeconds - delta);
        m_targetHoldRemainingSeconds =
            std::max(0.0f, m_targetHoldRemainingSeconds - delta);

        if (m_decisionRemainingSeconds > 0.0f) {
            return false;
        }

        const float baseInterval = std::max(
            0.05f,
            m_difficulty.decisionIntervalSeconds
        );
        const float humanVariation = 0.85f + m_random.NextUnitFloat() * 0.3f;
        m_decisionRemainingSeconds = baseInterval * humanVariation;
        return true;
    }

    bool CanChangeTarget() const noexcept {
        return !m_hasTarget || m_targetHoldRemainingSeconds <= 0.0f;
    }

    void CommitTarget() noexcept {
        m_hasTarget = true;
        const float variation = 0.85f + m_random.NextUnitFloat() * 0.3f;
        m_targetHoldRemainingSeconds =
            std::max(0.1f, m_difficulty.targetHoldSeconds) * variation;
    }

    void ClearTarget() noexcept {
        m_hasTarget = false;
        m_targetHoldRemainingSeconds = 0.0f;
    }

    bool ShouldMakeMistake() noexcept {
        return m_random.NextUnitFloat() < std::clamp(
            m_difficulty.mistakeProbability,
            0.0f,
            1.0f
        );
    }

    float GetDecisionRemainingSeconds() const noexcept {
        return m_decisionRemainingSeconds;
    }

    float GetTargetHoldRemainingSeconds() const noexcept {
        return m_targetHoldRemainingSeconds;
    }

private:
    CpuDifficultyProfile m_difficulty;
    DeterministicRandom m_random;
    float m_decisionRemainingSeconds = 0.0f;
    float m_targetHoldRemainingSeconds = 0.0f;
    bool m_hasTarget = false;
};

} // namespace MiniGameCollection
