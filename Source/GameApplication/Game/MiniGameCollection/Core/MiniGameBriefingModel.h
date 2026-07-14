#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace MiniGameCollection {

enum class BriefingMode : std::uint8_t {
    Full,
    Compact
};

enum class BriefingPhase : std::uint8_t {
    Inactive,
    AwaitingSkipRelease,
    StepActive,
    Ready,
    Complete
};

enum class BriefingEvent : std::uint32_t {
    None = 0,
    SkipArmed = 1u << 0,
    StepStarted = 1u << 1,
    StepCompleted = 1u << 2,
    StepReset = 1u << 3,
    Skipped = 1u << 4,
    ReadyReached = 1u << 5,
    Completed = 1u << 6
};

constexpr BriefingEvent operator|(
    BriefingEvent lhs,
    BriefingEvent rhs
) noexcept {
    return static_cast<BriefingEvent>(
        static_cast<std::uint32_t>(lhs) |
        static_cast<std::uint32_t>(rhs)
    );
}

constexpr BriefingEvent& operator|=(
    BriefingEvent& lhs,
    BriefingEvent rhs
) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool HasBriefingEvent(
    BriefingEvent events,
    BriefingEvent expected
) noexcept {
    return (
        static_cast<std::uint32_t>(events) &
        static_cast<std::uint32_t>(expected)
    ) != 0u;
}

struct BriefingStepDefinition {
    std::string prompt;
    float minimumDisplaySeconds = 0.0f;
    bool includedInCompactMode = true;
};

struct BriefingSkipSettings {
    float holdSeconds = 1.0f;
    bool requireReleaseToArm = true;
};

struct BriefingInputSample {
    bool stepSucceeded = false;
    bool resetRequested = false;
    bool skipKeyHeld = false;
};

class MiniGameBriefingModel final {
public:
    explicit MiniGameBriefingModel(
        std::vector<BriefingStepDefinition> steps = {},
        BriefingSkipSettings skipSettings = {}
    )
        : m_sourceSteps(std::move(steps)),
          m_skipSettings(NormalizeSkipSettings(skipSettings)) {
        NormalizeSteps(m_sourceSteps);
    }

    void SetSteps(std::vector<BriefingStepDefinition> steps) {
        NormalizeSteps(steps);
        m_sourceSteps = std::move(steps);
        Clear();
    }

    void SetSkipSettings(BriefingSkipSettings settings) noexcept {
        m_skipSettings = NormalizeSkipSettings(settings);
        m_skipHeldSeconds = 0.0f;
    }

    BriefingEvent Begin(BriefingMode mode) {
        m_mode = mode;
        m_activeSteps.clear();
        m_activeSteps.reserve(m_sourceSteps.size());
        for (const BriefingStepDefinition& step : m_sourceSteps) {
            if (mode == BriefingMode::Full || step.includedInCompactMode) {
                m_activeSteps.push_back(step);
            }
        }

        m_currentStepIndex = 0;
        m_stepElapsedSeconds = 0.0f;
        m_skipHeldSeconds = 0.0f;
        m_wasSkipped = false;
        m_skipArmed = !m_skipSettings.requireReleaseToArm;

        if (m_activeSteps.empty()) {
            m_phase = BriefingPhase::Ready;
            return BriefingEvent::ReadyReached;
        }

        m_phase = m_skipArmed
            ? BriefingPhase::StepActive
            : BriefingPhase::AwaitingSkipRelease;
        return m_skipArmed
            ? BriefingEvent::StepStarted
            : BriefingEvent::None;
    }

    BriefingEvent Tick(
        float deltaTime,
        const BriefingInputSample& input
    ) noexcept {
        const float delta = std::max(0.0f, deltaTime);

        if (m_phase == BriefingPhase::AwaitingSkipRelease) {
            if (!input.skipKeyHeld) {
                m_skipArmed = true;
                m_phase = BriefingPhase::StepActive;
                return BriefingEvent::SkipArmed |
                    BriefingEvent::StepStarted;
            }
            return BriefingEvent::None;
        }

        if (m_phase != BriefingPhase::StepActive) {
            return BriefingEvent::None;
        }

        m_stepElapsedSeconds += delta;

        if (input.resetRequested) {
            m_stepElapsedSeconds = 0.0f;
            m_skipHeldSeconds = 0.0f;
            return BriefingEvent::StepReset;
        }

        if (m_skipArmed && input.skipKeyHeld) {
            m_skipHeldSeconds = std::min(
                m_skipSettings.holdSeconds,
                m_skipHeldSeconds + delta
            );
            if (m_skipHeldSeconds >= m_skipSettings.holdSeconds) {
                m_wasSkipped = true;
                m_phase = BriefingPhase::Ready;
                return BriefingEvent::Skipped |
                    BriefingEvent::ReadyReached;
            }
        } else {
            m_skipHeldSeconds = 0.0f;
        }

        const BriefingStepDefinition* step = GetCurrentStep();
        if (!step || !input.stepSucceeded ||
            m_stepElapsedSeconds < step->minimumDisplaySeconds) {
            return BriefingEvent::None;
        }

        BriefingEvent events = BriefingEvent::StepCompleted;
        ++m_currentStepIndex;
        m_stepElapsedSeconds = 0.0f;
        m_skipHeldSeconds = 0.0f;

        if (m_currentStepIndex >= m_activeSteps.size()) {
            m_phase = BriefingPhase::Ready;
            events |= BriefingEvent::ReadyReached;
        } else {
            events |= BriefingEvent::StepStarted;
        }
        return events;
    }

    BriefingEvent ConfirmReady() noexcept {
        if (m_phase != BriefingPhase::Ready) {
            return BriefingEvent::None;
        }
        m_phase = BriefingPhase::Complete;
        return BriefingEvent::Completed;
    }

    void Clear() noexcept {
        m_activeSteps.clear();
        m_mode = BriefingMode::Full;
        m_phase = BriefingPhase::Inactive;
        m_currentStepIndex = 0;
        m_stepElapsedSeconds = 0.0f;
        m_skipHeldSeconds = 0.0f;
        m_skipArmed = false;
        m_wasSkipped = false;
    }

    BriefingMode GetMode() const noexcept { return m_mode; }
    BriefingPhase GetPhase() const noexcept { return m_phase; }
    bool IsActive() const noexcept {
        return m_phase == BriefingPhase::AwaitingSkipRelease ||
            m_phase == BriefingPhase::StepActive ||
            m_phase == BriefingPhase::Ready;
    }
    bool IsReady() const noexcept { return m_phase == BriefingPhase::Ready; }
    bool IsComplete() const noexcept { return m_phase == BriefingPhase::Complete; }
    bool IsSkipArmed() const noexcept { return m_skipArmed; }
    bool WasSkipped() const noexcept { return m_wasSkipped; }

    std::size_t GetCurrentStepIndex() const noexcept {
        return m_currentStepIndex;
    }

    std::size_t GetStepCount() const noexcept {
        return m_activeSteps.size();
    }

    const BriefingStepDefinition* GetCurrentStep() const noexcept {
        return m_currentStepIndex < m_activeSteps.size()
            ? &m_activeSteps[m_currentStepIndex]
            : nullptr;
    }

    const std::string& GetCurrentPrompt() const noexcept {
        static const std::string Empty;
        const BriefingStepDefinition* step = GetCurrentStep();
        return step ? step->prompt : Empty;
    }

    float GetStepProgress() const noexcept {
        const BriefingStepDefinition* step = GetCurrentStep();
        if (!step) {
            return m_phase == BriefingPhase::Ready ||
                m_phase == BriefingPhase::Complete
                ? 1.0f
                : 0.0f;
        }
        if (step->minimumDisplaySeconds <= 0.0f) {
            return 1.0f;
        }
        return std::clamp(
            m_stepElapsedSeconds / step->minimumDisplaySeconds,
            0.0f,
            1.0f
        );
    }

    float GetSkipProgress() const noexcept {
        if (!m_skipArmed ||
            m_phase != BriefingPhase::StepActive ||
            m_skipSettings.holdSeconds <= 0.0f) {
            return 0.0f;
        }
        return std::clamp(
            m_skipHeldSeconds / m_skipSettings.holdSeconds,
            0.0f,
            1.0f
        );
    }

    float GetSkipHoldSeconds() const noexcept {
        return m_skipSettings.holdSeconds;
    }

private:
    static BriefingSkipSettings NormalizeSkipSettings(
        BriefingSkipSettings settings
    ) noexcept {
        settings.holdSeconds = std::max(0.05f, settings.holdSeconds);
        return settings;
    }

    static void NormalizeSteps(
        std::vector<BriefingStepDefinition>& steps
    ) noexcept {
        for (BriefingStepDefinition& step : steps) {
            step.minimumDisplaySeconds = std::max(
                0.0f,
                step.minimumDisplaySeconds
            );
        }
    }

    std::vector<BriefingStepDefinition> m_sourceSteps;
    std::vector<BriefingStepDefinition> m_activeSteps;
    BriefingSkipSettings m_skipSettings{};
    BriefingMode m_mode = BriefingMode::Full;
    BriefingPhase m_phase = BriefingPhase::Inactive;
    std::size_t m_currentStepIndex = 0;
    float m_stepElapsedSeconds = 0.0f;
    float m_skipHeldSeconds = 0.0f;
    bool m_skipArmed = false;
    bool m_wasSkipped = false;
};

class MiniGameBriefingSessionState final {
public:
    bool HasCompleted(MiniGameId gameId) const noexcept {
        const std::size_t index = ToIndex(gameId);
        return index < m_completed.size() && m_completed[index];
    }

    void MarkCompleted(MiniGameId gameId) noexcept {
        const std::size_t index = ToIndex(gameId);
        if (index < m_completed.size()) {
            m_completed[index] = true;
        }
    }

    BriefingMode ResolveMode(
        MiniGameId gameId,
        bool isRetry
    ) const noexcept {
        return isRetry || HasCompleted(gameId)
            ? BriefingMode::Compact
            : BriefingMode::Full;
    }

    void Reset() noexcept {
        m_completed.fill(false);
    }

private:
    static constexpr std::size_t ToIndex(MiniGameId gameId) noexcept {
        switch (gameId) {
        case MiniGameId::PresentationSpike: return 0;
        case MiniGameId::ColorTerritory: return 1;
        case MiniGameId::SheepRoundup: return 2;
        case MiniGameId::Backshot: return 3;
        }
        return 4;
    }

    std::array<bool, 4> m_completed{};
};

} // namespace MiniGameCollection
