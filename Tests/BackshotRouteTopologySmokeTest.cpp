#include "Game/MiniGameCollection/Backshot/BackshotDynamicEvents.h"
#include "Game/MiniGameCollection/Backshot/BackshotRouteTopology.h"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

using namespace MiniGameCollection::Backshot;

void TestCornerAutoTurn() {
    BackshotRouteLayoutBuilder builder("CORNER", 4, 4, 1.0f, 1u);
    const BackshotRouteLayout layout = builder
        .ConnectPath({{0, 1}, {1, 1}, {2, 1}, {2, 2}, {2, 3}})
        .SetPlayerStarts({SlideCell{0, 1}, {2, 3}, {0, 1}, {2, 3}})
        .Build();
    BackshotRouteBoard board(layout);

    const RouteSlideMove move = board.ComputeMove(
        {0, 1},
        SlideDirection::Right,
        {}
    );
    assert(move.IsValid());
    assert(move.stop == SlideCell{2, 3});
    assert(move.finalDirection == SlideDirection::Up);
    assert(move.DistanceCells() == 4);
    assert(move.path[2] == SlideCell{2, 1});
    assert(move.path[3] == SlideCell{2, 2});
}

void TestJunctionStopsAndInvalidConnection() {
    BackshotRouteBoard board(BackshotRouteLayouts::LayoutC());
    const RouteSlideMove move = board.ComputeMove(
        {1, 3},
        SlideDirection::Right,
        {}
    );
    assert(move.IsValid());
    assert(move.stop == SlideCell{3, 3});

    const RouteSlideMove invalid = board.ComputeMove(
        {1, 1},
        SlideDirection::Down,
        {}
    );
    assert(!invalid.IsValid());
    assert(invalid.stop == SlideCell{1, 1});
}

void TestReservedAndTemporaryBlockStopsBeforeCell() {
    BackshotRouteBoard board(BackshotRouteLayouts::LayoutA());
    RouteSlideMove move = board.ComputeMove(
        {1, 1},
        SlideDirection::Right,
        {{4, 1}}
    );
    assert(move.IsValid());
    assert(move.stop == SlideCell{3, 1});

    board.SetTemporaryBlockedCells({{4, 1}});
    move = board.ComputeMove({1, 1}, SlideDirection::Right, {});
    assert(move.stop == SlideCell{3, 1});
}

void TestLayoutsAreDeterministicAndDistinct() {
    const BackshotRouteLayout first = BackshotRouteLayouts::ForRound(0);
    const BackshotRouteLayout again = BackshotRouteLayouts::ForRound(3);
    const BackshotRouteLayout second = BackshotRouteLayouts::ForRound(1);
    const BackshotRouteLayout third = BackshotRouteLayouts::ForRound(2);

    assert(first.name == again.name);
    assert(first.deterministicSeed == again.deterministicSeed);
    assert(first.cells.size() == again.cells.size());
    for (std::size_t index = 0; index < first.cells.size(); ++index) {
        assert(first.cells[index].connections == again.cells[index].connections);
    }
    assert(first.name != second.name);
    assert(second.name != third.name);
    assert(first.deterministicSeed != second.deterministicSeed);
}

void TestBoostDurationAndNonStacking() {
    BackshotBoostConfig config;
    BackshotBoostState boost;
    boost.Activate(config);
    assert(boost.IsActive());
    assert(std::abs(boost.RemainingSeconds() - 5.0f) < 0.0001f);
    assert(std::abs(boost.ResolveSpeedMultiplier(config) - 1.4f) < 0.0001f);

    boost.Tick(2.0f);
    boost.Activate(config);
    assert(std::abs(boost.RemainingSeconds() - 5.0f) < 0.0001f);
    boost.Activate(config);
    assert(std::abs(boost.RemainingSeconds() - 5.0f) < 0.0001f);

    const float boosted = boost.ResolveSlideDuration(
        6,
        0.072f,
        0.18f,
        0.52f,
        config
    );
    assert(boosted >= config.minimumSlideSeconds);
    assert(boosted < 6.0f * 0.072f);

    boost.Tick(5.0f);
    assert(!boost.IsActive());
    assert(std::abs(boost.ResolveSpeedMultiplier(config) - 1.0f) < 0.0001f);
}

void TestTemporaryBlockLifecycleAndSafety() {
    BackshotRouteBoard board(BackshotRouteLayouts::LayoutA());
    TemporaryRouteBlockerModel blocker;
    const std::vector<SlideCell> occupied{
        {1, 1}, {9, 1}, {9, 5}, {1, 5}
    };

    assert(!blocker.Schedule({1, 1}, board, occupied, {}));
    assert(!blocker.Schedule({5, 3}, board, occupied, {{5, 3}}));
    assert(blocker.Schedule(
        {5, 3},
        board,
        occupied,
        {},
        {.warningSeconds = 4.0f, .closedSeconds = 6.0f, .reopeningSeconds = 1.0f}
    ));
    assert(blocker.Phase() == TemporaryBlockPhase::Warning);
    assert(blocker.BlocksNewRoutes());
    assert(!blocker.IsPhysicallyClosed());

    blocker.Tick(4.0f);
    assert(blocker.Phase() == TemporaryBlockPhase::Closed);
    assert(blocker.IsPhysicallyClosed());

    blocker.Tick(6.0f);
    assert(blocker.Phase() == TemporaryBlockPhase::Reopening);
    assert(blocker.IsPhysicallyClosed());

    blocker.Tick(1.0f);
    assert(blocker.Phase() == TemporaryBlockPhase::Inactive);
    assert(!blocker.Cell().has_value());

    const auto events = blocker.ConsumeEvents();
    assert(events.size() == 4);
    assert(events[0].type == TemporaryBlockEvent::Type::WarningStarted);
    assert(events[1].type == TemporaryBlockEvent::Type::Closed);
    assert(events[2].type == TemporaryBlockEvent::Type::Reopening);
    assert(events[3].type == TemporaryBlockEvent::Type::Reopened);
}

void TestBlockCannotStrandAllPlayers() {
    BackshotRouteLayoutBuilder builder("LINE", 3, 1, 1.0f, 5u);
    BackshotRouteBoard board(builder
        .ConnectPath({{0, 0}, {1, 0}, {2, 0}})
        .SetPlayerStarts({SlideCell{0, 0}, {2, 0}, {0, 0}, {2, 0}})
        .Build());

    assert(!board.WouldRemainNavigable(
        {1, 0},
        {{0, 0}, {2, 0}}
    ));
}

void TestDeterministicSelector() {
    BackshotRouteBoard board(BackshotRouteLayouts::LayoutB());
    BackshotDeterministicCellSelector lhs(12345u);
    BackshotDeterministicCellSelector rhs(12345u);
    const std::vector<SlideCell> excluded{{2, 1}, {8, 1}};

    for (int index = 0; index < 8; ++index) {
        assert(lhs.Choose(board, excluded) == rhs.Choose(board, excluded));
    }
}

} // namespace

int main() {
    TestCornerAutoTurn();
    TestJunctionStopsAndInvalidConnection();
    TestReservedAndTemporaryBlockStopsBeforeCell();
    TestLayoutsAreDeterministicAndDistinct();
    TestBoostDurationAndNonStacking();
    TestTemporaryBlockLifecycleAndSafety();
    TestBlockCannotStrandAllPlayers();
    TestDeterministicSelector();
    return 0;
}
