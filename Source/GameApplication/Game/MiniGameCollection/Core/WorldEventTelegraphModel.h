#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"
#include "Game/MiniGameCollection/Core/MiniGameMath.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace MiniGameCollection {

enum class TelegraphPriority : std::uint8_t {
    Minor,
    Major
};

enum class TelegraphShape : std::uint8_t {
    Point,
    Ring,
    Area,
    Path,
    Screen
};

enum class TelegraphPhase : std::uint8_t {
    Pending,
    Warning,
    Armed,
    Resolving,
    Aftermath,
    Complete
};

enum class TelegraphEventType : std::uint8_t {
    Activated,
    Armed,
    Resolve,
    Aftermath,
    Completed,
    Cancelled
};

struct TelegraphDefinition {
    std::uint64_t id = 0;
    SceneToken sceneToken = 0;
    TelegraphPriority priority = TelegraphPriority::Minor;
    Vec2 worldPosition{};
    TelegraphShape shape = TelegraphShape::Point;
    float radius = 0.0f;
    float warningSeconds = 0.0f;
    float armedSeconds = 0.0f;
    float resolvingSeconds = 0.0f;
    float aftermathSeconds = 0.0f;
    std::string label;
};

struct TelegraphEvent {
    TelegraphEventType type = TelegraphEventType::Activated;
    std::uint64_t id = 0;
    SceneToken sceneToken = 0;
    TelegraphPriority priority = TelegraphPriority::Minor;
};

struct TelegraphSnapshot {
    TelegraphDefinition definition;
    TelegraphPhase phase = TelegraphPhase::Pending;
    float phaseRemainingSeconds = 0.0f;
    float phaseDurationSeconds = 0.0f;
    float totalRemainingSeconds = 0.0f;
    float phaseProgress = 0.0f;
};

class WorldEventTelegraphModel final {
public:
    static constexpr std::size_t MajorDisplayLimit = 1;
    static constexpr std::size_t MinorDisplayLimit = 2;

    bool Submit(TelegraphDefinition definition) {
        Normalize(definition);
        if (definition.id == 0 || definition.sceneToken == 0 ||
            definition.label.empty() || Contains(definition.id)) {
            return false;
        }

        m_entries.push_back({
            .definition = std::move(definition),
            .phase = TelegraphPhase::Pending,
            .phaseRemainingSeconds = 0.0f,
            .phaseDurationSeconds = 0.0f,
            .serial = m_nextSerial++
        });
        ActivateAvailable();
        return true;
    }

    std::vector<TelegraphEvent> Tick(float deltaTime) {
        const float delta = (std::max)(0.0f, deltaTime);
        std::vector<TelegraphEvent> events;
        ActivateAvailable(&events);

        for (Entry& entry : m_entries) {
            if (!IsActivePhase(entry.phase)) {
                continue;
            }

            float remainingDelta = delta;
            while (remainingDelta >= 0.0f && IsActivePhase(entry.phase)) {
                if (entry.phaseRemainingSeconds > remainingDelta) {
                    entry.phaseRemainingSeconds -= remainingDelta;
                    break;
                }

                remainingDelta -= entry.phaseRemainingSeconds;
                entry.phaseRemainingSeconds = 0.0f;
                Advance(entry, events);

                if (remainingDelta <= 0.0f) {
                    break;
                }
            }
        }

        ActivateAvailable(&events);
        CompactCompleted();
        return events;
    }

    bool Cancel(std::uint64_t id) {
        for (Entry& entry : m_entries) {
            if (entry.definition.id != id ||
                entry.phase == TelegraphPhase::Complete) {
                continue;
            }
            entry.phase = TelegraphPhase::Complete;
            entry.phaseRemainingSeconds = 0.0f;
            entry.phaseDurationSeconds = 0.0f;
            m_deferredEvents.push_back({
                .type = TelegraphEventType::Cancelled,
                .id = entry.definition.id,
                .sceneToken = entry.definition.sceneToken,
                .priority = entry.definition.priority
            });
            ActivateAvailable();
            return true;
        }
        return false;
    }

    void ClearForScene(SceneToken sceneToken) {
        for (Entry& entry : m_entries) {
            if (entry.definition.sceneToken == sceneToken &&
                entry.phase != TelegraphPhase::Complete) {
                entry.phase = TelegraphPhase::Complete;
                m_deferredEvents.push_back({
                    .type = TelegraphEventType::Cancelled,
                    .id = entry.definition.id,
                    .sceneToken = sceneToken,
                    .priority = entry.definition.priority
                });
            }
        }
        CompactCompleted();
        ActivateAvailable();
    }

    void Clear() noexcept {
        m_entries.clear();
        m_deferredEvents.clear();
        m_nextSerial = 1;
    }

    std::vector<TelegraphEvent> ConsumeDeferredEvents() {
        std::vector<TelegraphEvent> events;
        events.swap(m_deferredEvents);
        return events;
    }

    std::vector<TelegraphSnapshot> GetVisibleSnapshots() const {
        std::vector<TelegraphSnapshot> snapshots;
        for (const Entry& entry : m_entries) {
            if (!IsActivePhase(entry.phase)) {
                continue;
            }
            snapshots.push_back(BuildSnapshot(entry));
        }
        std::stable_sort(
            snapshots.begin(),
            snapshots.end(),
            [](const TelegraphSnapshot& lhs, const TelegraphSnapshot& rhs) {
                return static_cast<int>(lhs.definition.priority) >
                    static_cast<int>(rhs.definition.priority);
            }
        );
        return snapshots;
    }

    std::optional<TelegraphSnapshot> Find(std::uint64_t id) const {
        for (const Entry& entry : m_entries) {
            if (entry.definition.id == id &&
                entry.phase != TelegraphPhase::Complete) {
                return BuildSnapshot(entry);
            }
        }
        return std::nullopt;
    }

    bool HasActiveMajor() const noexcept {
        return ActiveCount(TelegraphPriority::Major) > 0;
    }

    std::size_t ActiveCount(TelegraphPriority priority) const noexcept {
        return static_cast<std::size_t>(std::count_if(
            m_entries.begin(),
            m_entries.end(),
            [priority](const Entry& entry) {
                return entry.definition.priority == priority &&
                    IsActivePhase(entry.phase);
            }
        ));
    }

    std::size_t PendingCount() const noexcept {
        return static_cast<std::size_t>(std::count_if(
            m_entries.begin(),
            m_entries.end(),
            [](const Entry& entry) {
                return entry.phase == TelegraphPhase::Pending;
            }
        ));
    }

    bool Empty() const noexcept {
        return m_entries.empty();
    }

private:
    struct Entry {
        TelegraphDefinition definition;
        TelegraphPhase phase = TelegraphPhase::Pending;
        float phaseRemainingSeconds = 0.0f;
        float phaseDurationSeconds = 0.0f;
        std::uint64_t serial = 0;
    };

    static void Normalize(TelegraphDefinition& definition) noexcept {
        definition.radius = (std::max)(0.0f, definition.radius);
        definition.warningSeconds = (std::max)(0.0f, definition.warningSeconds);
        definition.armedSeconds = (std::max)(0.0f, definition.armedSeconds);
        definition.resolvingSeconds = (std::max)(0.0f, definition.resolvingSeconds);
        definition.aftermathSeconds = (std::max)(0.0f, definition.aftermathSeconds);
    }

    bool Contains(std::uint64_t id) const noexcept {
        return std::any_of(
            m_entries.begin(),
            m_entries.end(),
            [id](const Entry& entry) {
                return entry.definition.id == id &&
                    entry.phase != TelegraphPhase::Complete;
            }
        );
    }

    static bool IsActivePhase(TelegraphPhase phase) noexcept {
        return phase == TelegraphPhase::Warning ||
            phase == TelegraphPhase::Armed ||
            phase == TelegraphPhase::Resolving ||
            phase == TelegraphPhase::Aftermath;
    }

    std::size_t LimitFor(TelegraphPriority priority) const noexcept {
        return priority == TelegraphPriority::Major
            ? MajorDisplayLimit
            : MinorDisplayLimit;
    }

    void ActivateAvailable(std::vector<TelegraphEvent>* events = nullptr) {
        for (TelegraphPriority priority : {
            TelegraphPriority::Major,
            TelegraphPriority::Minor
        }) {
            while (ActiveCount(priority) < LimitFor(priority)) {
                Entry* selected = nullptr;
                for (Entry& entry : m_entries) {
                    if (entry.phase != TelegraphPhase::Pending ||
                        entry.definition.priority != priority) {
                        continue;
                    }
                    if (!selected || entry.serial < selected->serial) {
                        selected = &entry;
                    }
                }
                if (!selected) {
                    break;
                }
                EnterPhase(*selected, TelegraphPhase::Warning);
                TelegraphEvent activated{
                    .type = TelegraphEventType::Activated,
                    .id = selected->definition.id,
                    .sceneToken = selected->definition.sceneToken,
                    .priority = selected->definition.priority
                };
                if (events) {
                    events->push_back(activated);
                } else {
                    m_deferredEvents.push_back(activated);
                }
            }
        }
    }

    static void EnterPhase(Entry& entry, TelegraphPhase phase) noexcept {
        entry.phase = phase;
        switch (phase) {
        case TelegraphPhase::Warning:
            entry.phaseDurationSeconds = entry.definition.warningSeconds;
            break;
        case TelegraphPhase::Armed:
            entry.phaseDurationSeconds = entry.definition.armedSeconds;
            break;
        case TelegraphPhase::Resolving:
            entry.phaseDurationSeconds = entry.definition.resolvingSeconds;
            break;
        case TelegraphPhase::Aftermath:
            entry.phaseDurationSeconds = entry.definition.aftermathSeconds;
            break;
        default:
            entry.phaseDurationSeconds = 0.0f;
            break;
        }
        entry.phaseRemainingSeconds = entry.phaseDurationSeconds;
    }

    static void Advance(
        Entry& entry,
        std::vector<TelegraphEvent>& events
    ) {
        TelegraphEvent event{
            .id = entry.definition.id,
            .sceneToken = entry.definition.sceneToken,
            .priority = entry.definition.priority
        };

        switch (entry.phase) {
        case TelegraphPhase::Warning:
            EnterPhase(entry, TelegraphPhase::Armed);
            event.type = TelegraphEventType::Armed;
            events.push_back(event);
            break;
        case TelegraphPhase::Armed:
            EnterPhase(entry, TelegraphPhase::Resolving);
            event.type = TelegraphEventType::Resolve;
            events.push_back(event);
            break;
        case TelegraphPhase::Resolving:
            EnterPhase(entry, TelegraphPhase::Aftermath);
            event.type = TelegraphEventType::Aftermath;
            events.push_back(event);
            break;
        case TelegraphPhase::Aftermath:
            entry.phase = TelegraphPhase::Complete;
            event.type = TelegraphEventType::Completed;
            events.push_back(event);
            break;
        default:
            break;
        }
    }

    static float RemainingAfterCurrent(const Entry& entry) noexcept {
        switch (entry.phase) {
        case TelegraphPhase::Warning:
            return entry.definition.armedSeconds +
                entry.definition.resolvingSeconds +
                entry.definition.aftermathSeconds;
        case TelegraphPhase::Armed:
            return entry.definition.resolvingSeconds +
                entry.definition.aftermathSeconds;
        case TelegraphPhase::Resolving:
            return entry.definition.aftermathSeconds;
        default:
            return 0.0f;
        }
    }

    static TelegraphSnapshot BuildSnapshot(const Entry& entry) noexcept {
        const float progress = entry.phaseDurationSeconds > 0.0f
            ? 1.0f - entry.phaseRemainingSeconds /
                entry.phaseDurationSeconds
            : 1.0f;
        return {
            .definition = entry.definition,
            .phase = entry.phase,
            .phaseRemainingSeconds = entry.phaseRemainingSeconds,
            .phaseDurationSeconds = entry.phaseDurationSeconds,
            .totalRemainingSeconds = entry.phaseRemainingSeconds +
                RemainingAfterCurrent(entry),
            .phaseProgress = (std::clamp)(progress, 0.0f, 1.0f)
        };
    }

    void CompactCompleted() {
        std::erase_if(
            m_entries,
            [](const Entry& entry) {
                return entry.phase == TelegraphPhase::Complete;
            }
        );
    }

    std::vector<Entry> m_entries;
    std::vector<TelegraphEvent> m_deferredEvents;
    std::uint64_t m_nextSerial = 1;
};

} // namespace MiniGameCollection
