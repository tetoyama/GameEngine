#include "Game/MiniGameCollection/Core/WorldEventTelegraphModel.h"

#include <cassert>
#include <cstddef>
#include <vector>

namespace {

using namespace MiniGameCollection;

TelegraphDefinition MakeDefinition(
    std::uint64_t id,
    SceneToken sceneToken,
    TelegraphPriority priority,
    const char* label,
    float warning = 1.0f,
    float armed = 0.25f,
    float resolving = 0.1f,
    float aftermath = 0.5f
) {
    return {
        .id = id,
        .sceneToken = sceneToken,
        .priority = priority,
        .worldPosition = {1.0f, 2.0f},
        .shape = TelegraphShape::Ring,
        .radius = 2.0f,
        .warningSeconds = warning,
        .armedSeconds = armed,
        .resolvingSeconds = resolving,
        .aftermathSeconds = aftermath,
        .label = label
    };
}

void TestPhaseOrderAndResolveTiming() {
    WorldEventTelegraphModel model;
    assert(model.Submit(MakeDefinition(
        1,
        77,
        TelegraphPriority::Major,
        "BOMB"
    )));
    assert(!model.Submit(MakeDefinition(
        1,
        77,
        TelegraphPriority::Major,
        "DUPLICATE"
    )));

    auto snapshot = model.Find(1);
    assert(snapshot.has_value());
    assert(snapshot->phase == TelegraphPhase::Warning);
    assert(model.HasActiveMajor());

    auto events = model.Tick(0.9f);
    assert(events.empty());
    snapshot = model.Find(1);
    assert(snapshot->phase == TelegraphPhase::Warning);

    events = model.Tick(0.1f);
    assert(events.size() == 1);
    assert(events[0].type == TelegraphEventType::Armed);
    snapshot = model.Find(1);
    assert(snapshot->phase == TelegraphPhase::Armed);

    events = model.Tick(0.25f);
    assert(events.size() == 1);
    assert(events[0].type == TelegraphEventType::Resolve);
    snapshot = model.Find(1);
    assert(snapshot->phase == TelegraphPhase::Resolving);

    events = model.Tick(0.1f);
    assert(events.size() == 1);
    assert(events[0].type == TelegraphEventType::Aftermath);
    snapshot = model.Find(1);
    assert(snapshot->phase == TelegraphPhase::Aftermath);

    events = model.Tick(0.5f);
    assert(events.size() == 1);
    assert(events[0].type == TelegraphEventType::Completed);
    assert(!model.Find(1).has_value());
    assert(!model.HasActiveMajor());
}

void TestPriorityAndDisplayLimits() {
    WorldEventTelegraphModel model;
    assert(model.Submit(MakeDefinition(1, 10, TelegraphPriority::Major, "M1")));
    assert(model.Submit(MakeDefinition(2, 10, TelegraphPriority::Major, "M2")));
    assert(model.Submit(MakeDefinition(3, 10, TelegraphPriority::Minor, "m1")));
    assert(model.Submit(MakeDefinition(4, 10, TelegraphPriority::Minor, "m2")));
    assert(model.Submit(MakeDefinition(5, 10, TelegraphPriority::Minor, "m3")));

    assert(model.ActiveCount(TelegraphPriority::Major) == 1);
    assert(model.ActiveCount(TelegraphPriority::Minor) == 2);
    assert(model.PendingCount() == 2);

    const auto visible = model.GetVisibleSnapshots();
    assert(visible.size() == 3);
    assert(visible.front().definition.priority == TelegraphPriority::Major);

    model.Tick(2.0f);
    assert(model.ActiveCount(TelegraphPriority::Major) == 1);
    assert(model.ActiveCount(TelegraphPriority::Minor) == 1);
    assert(model.PendingCount() == 0);
    assert(model.Find(2).has_value());
    assert(model.Find(5).has_value());
}

void TestSceneCleanupAndCancellation() {
    WorldEventTelegraphModel model;
    assert(model.Submit(MakeDefinition(1, 100, TelegraphPriority::Major, "A")));
    assert(model.Submit(MakeDefinition(2, 200, TelegraphPriority::Minor, "B")));
    assert(model.Submit(MakeDefinition(3, 100, TelegraphPriority::Minor, "C")));

    model.ClearForScene(100);
    assert(!model.Find(1).has_value());
    assert(!model.Find(3).has_value());
    assert(model.Find(2).has_value());

    auto events = model.ConsumeDeferredEvents();
    std::size_t cancelled = 0;
    for (const TelegraphEvent& event : events) {
        if (event.type == TelegraphEventType::Cancelled &&
            event.sceneToken == 100) {
            ++cancelled;
        }
    }
    assert(cancelled == 2);

    assert(model.Cancel(2));
    assert(!model.Find(2).has_value());
    model.Clear();
    assert(model.Empty());
}

void TestZeroDurationPhases() {
    WorldEventTelegraphModel model;
    assert(model.Submit(MakeDefinition(
        9,
        300,
        TelegraphPriority::Minor,
        "INSTANT",
        0.0f,
        0.0f,
        0.0f,
        0.0f
    )));

    const auto events = model.Tick(0.0f);
    assert(events.size() == 4);
    assert(events[0].type == TelegraphEventType::Armed);
    assert(events[1].type == TelegraphEventType::Resolve);
    assert(events[2].type == TelegraphEventType::Aftermath);
    assert(events[3].type == TelegraphEventType::Completed);
    assert(model.Empty());
}

} // namespace

int main() {
    TestPhaseOrderAndResolveTiming();
    TestPriorityAndDisplayLimits();
    TestSceneCleanupAndCancellation();
    TestZeroDurationPhases();
    return 0;
}
