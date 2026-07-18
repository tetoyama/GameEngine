#pragma once

#include "Game/MiniGameCollection/Backshot/BackshotRouteTopology.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace MiniGameCollection::Backshot {

struct BackshotBoostConfig {
    float durationSeconds = 5.0f;
    float speedMultiplier = 1.4f;
    float minimumSlideSeconds = 0.14f;
};

class BackshotBoostState final {
public:
    void Activate(const BackshotBoostConfig& config) noexcept {
        m_remainingSeconds = (std::max)(
            m_remainingSeconds,
            (std::max)(0.0f, config.durationSeconds)
        );
    }

    void Tick(float deltaTime) noexcept {
        m_remainingSeconds = (std::max)(
            0.0f,
            m_remainingSeconds - (std::max)(0.0f, deltaTime)
        );
    }

    void Reset() noexcept {
        m_remainingSeconds = 0.0f;
    }

    bool IsActive() const noexcept {
        return m_remainingSeconds > 0.0f;
    }

    float RemainingSeconds() const noexcept {
        return m_remainingSeconds;
    }

    float ResolveSpeedMultiplier(
        const BackshotBoostConfig& config
    ) const noexcept {
        return IsActive()
            ? (std::max)(1.0f, config.speedMultiplier)
            : 1.0f;
    }

    float ResolveSlideDuration(
        int distanceCells,
        float secondsPerCell,
        float normalMinimumSeconds,
        float maximumSeconds,
        const BackshotBoostConfig& config
    ) const noexcept {
        const float distanceDuration =
            static_cast<float>((std::max)(0, distanceCells)) *
            (std::max)(0.0f, secondsPerCell);
        const float multiplier = ResolveSpeedMultiplier(config);
        const float minimum = IsActive()
            ? (std::max)(0.01f, config.minimumSlideSeconds)
            : (std::max)(0.01f, normalMinimumSeconds);
        return (std::clamp)(
            distanceDuration / multiplier,
            minimum,
            (std::max)(minimum, maximumSeconds)
        );
    }

private:
    float m_remainingSeconds = 0.0f;
};

enum class TemporaryBlockPhase : std::uint8_t {
    Inactive,
    Warning,
    Closed,
    Reopening
};

struct TemporaryBlockConfig {
    float warningSeconds = 4.0f;
    float closedSeconds = 6.0f;
    float reopeningSeconds = 1.0f;
};

struct TemporaryBlockEvent {
    enum class Type : std::uint8_t {
        WarningStarted,
        Closed,
        Reopening,
        Reopened
    };

    Type type = Type::WarningStarted;
    SlideCell cell{};
};

class TemporaryRouteBlockerModel final {
public:
    bool Schedule(
        SlideCell cell,
        const BackshotRouteBoard& board,
        const std::vector<SlideCell>& occupiedCells,
        const std::vector<SlideCell>& reservedCells,
        TemporaryBlockConfig config = {}
    ) noexcept {
        if (m_phase != TemporaryBlockPhase::Inactive ||
            !board.IsRoute(cell) ||
            Contains(occupiedCells, cell) ||
            Contains(reservedCells, cell) ||
            !board.WouldRemainNavigable(cell, occupiedCells)) {
            return false;
        }

        m_cell = cell;
        m_config.warningSeconds = (std::max)(0.0f, config.warningSeconds);
        m_config.closedSeconds = (std::max)(0.0f, config.closedSeconds);
        m_config.reopeningSeconds = (std::max)(0.0f, config.reopeningSeconds);
        m_phase = TemporaryBlockPhase::Warning;
        m_remainingSeconds = m_config.warningSeconds;
        m_events.push_back({
            .type = TemporaryBlockEvent::Type::WarningStarted,
            .cell = cell
        });
        CollapseZeroDurationPhases();
        return true;
    }

    void Tick(float deltaTime) noexcept {
        float remainingDelta = (std::max)(0.0f, deltaTime);
        while (m_phase != TemporaryBlockPhase::Inactive) {
            if (m_remainingSeconds > remainingDelta) {
                m_remainingSeconds -= remainingDelta;
                return;
            }
            remainingDelta -= m_remainingSeconds;
            m_remainingSeconds = 0.0f;
            Advance();
            if (remainingDelta <= 0.0f &&
                (m_phase == TemporaryBlockPhase::Inactive ||
                 m_remainingSeconds > 0.0f)) {
                return;
            }
        }
    }

    void Reset() noexcept {
        m_phase = TemporaryBlockPhase::Inactive;
        m_cell.reset();
        m_remainingSeconds = 0.0f;
        m_events.clear();
    }

    TemporaryBlockPhase Phase() const noexcept { return m_phase; }
    float RemainingSeconds() const noexcept { return m_remainingSeconds; }
    bool IsActive() const noexcept {
        return m_phase != TemporaryBlockPhase::Inactive;
    }

    bool BlocksNewRoutes() const noexcept {
        // 安全実装: Warning開始時から新規経路上ではBlockとして扱う。
        return m_phase == TemporaryBlockPhase::Warning ||
            m_phase == TemporaryBlockPhase::Closed ||
            m_phase == TemporaryBlockPhase::Reopening;
    }

    bool IsPhysicallyClosed() const noexcept {
        return m_phase == TemporaryBlockPhase::Closed ||
            m_phase == TemporaryBlockPhase::Reopening;
    }

    std::optional<SlideCell> Cell() const noexcept { return m_cell; }

    std::vector<TemporaryBlockEvent> ConsumeEvents() {
        std::vector<TemporaryBlockEvent> result;
        result.swap(m_events);
        return result;
    }

private:
    static bool Contains(
        const std::vector<SlideCell>& cells,
        SlideCell cell
    ) noexcept {
        return std::find(cells.begin(), cells.end(), cell) != cells.end();
    }

    void CollapseZeroDurationPhases() noexcept {
        while (m_phase != TemporaryBlockPhase::Inactive &&
            m_remainingSeconds <= 0.0f) {
            Advance();
        }
    }

    void Advance() noexcept {
        if (!m_cell) {
            Reset();
            return;
        }

        switch (m_phase) {
        case TemporaryBlockPhase::Warning:
            m_phase = TemporaryBlockPhase::Closed;
            m_remainingSeconds = m_config.closedSeconds;
            m_events.push_back({
                .type = TemporaryBlockEvent::Type::Closed,
                .cell = *m_cell
            });
            break;
        case TemporaryBlockPhase::Closed:
            m_phase = TemporaryBlockPhase::Reopening;
            m_remainingSeconds = m_config.reopeningSeconds;
            m_events.push_back({
                .type = TemporaryBlockEvent::Type::Reopening,
                .cell = *m_cell
            });
            break;
        case TemporaryBlockPhase::Reopening:
            m_events.push_back({
                .type = TemporaryBlockEvent::Type::Reopened,
                .cell = *m_cell
            });
            m_phase = TemporaryBlockPhase::Inactive;
            m_cell.reset();
            m_remainingSeconds = 0.0f;
            break;
        default:
            Reset();
            break;
        }
    }

    TemporaryBlockConfig m_config{};
    TemporaryBlockPhase m_phase = TemporaryBlockPhase::Inactive;
    std::optional<SlideCell> m_cell;
    float m_remainingSeconds = 0.0f;
    std::vector<TemporaryBlockEvent> m_events;
};

struct BackshotDynamicEventScheduleConfig {
    float boostFirstRemainingSeconds = 20.0f;
    float boostRepeatSeconds = 7.0f;
    float blockerFirstRemainingSeconds = 16.0f;
    float blockerRepeatSeconds = 9.0f;
    std::size_t boostPoolCapacity = 2;
};

class BackshotDeterministicCellSelector final {
public:
    explicit BackshotDeterministicCellSelector(
        std::uint32_t seed = 0xBAAC5001u
    ) noexcept
        : m_state(seed == 0 ? 0xBAAC5001u : seed) {
    }

    std::optional<SlideCell> Choose(
        const BackshotRouteBoard& board,
        const std::vector<SlideCell>& excludedCells
    ) noexcept {
        std::vector<SlideCell> candidates = board.RouteCells();
        std::erase_if(
            candidates,
            [&excludedCells](SlideCell cell) {
                return std::find(
                    excludedCells.begin(),
                    excludedCells.end(),
                    cell
                ) != excludedCells.end();
            }
        );
        if (candidates.empty()) {
            return std::nullopt;
        }
        const std::size_t index = static_cast<std::size_t>(Next()) %
            candidates.size();
        return candidates[index];
    }

private:
    std::uint32_t Next() noexcept {
        std::uint32_t value = m_state;
        value ^= value << 13u;
        value ^= value >> 17u;
        value ^= value << 5u;
        m_state = value == 0 ? 0xBAAC5001u : value;
        return m_state;
    }

    std::uint32_t m_state = 0xBAAC5001u;
};

} // namespace MiniGameCollection::Backshot
