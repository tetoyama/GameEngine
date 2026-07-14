#include "Game/MiniGameCollection/Backshot/BackshotSlideMovement.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "Backshot slide contract failed: " << message << '\n';
        return false;
    }
    return true;
}

} // namespace

int main() {
    using namespace MiniGameCollection::Backshot;

    BackshotSlideBoard board(
        5,
        4,
        2.0f,
        std::vector<SlideCell>{{2, 1}}
    );

    bool ok = true;

    const SlideMove wallStop = board.ComputeMove(
        {0, 0},
        SlideDirection::Right,
        {}
    );
    ok &= Expect(wallStop.IsValid(), "wall slide must be valid");
    ok &= Expect(wallStop.stop == SlideCell{4, 0}, "slide must stop at board edge");
    ok &= Expect(wallStop.distanceCells == 4, "wall slide distance must match cells");

    const SlideMove blockStop = board.ComputeMove(
        {0, 1},
        SlideDirection::Right,
        {}
    );
    ok &= Expect(blockStop.stop == SlideCell{1, 1}, "fixed block must stop slide one cell before it");
    ok &= Expect(blockStop.distanceCells == 1, "fixed block stop distance must be deterministic");

    const SlideMove playerStop = board.ComputeMove(
        {0, 0},
        SlideDirection::Right,
        std::vector<SlideCell>{{3, 0}}
    );
    ok &= Expect(playerStop.stop == SlideCell{2, 0}, "reserved player cell must act as a stopper");

    const SlideMove noMove = board.ComputeMove(
        {1, 1},
        SlideDirection::Right,
        {}
    );
    ok &= Expect(!noMove.IsValid(), "adjacent obstacle must reject zero-distance slide");

    const std::vector<SlideCell> trace = board.TraceMove(playerStop);
    ok &= Expect(trace.size() == 3, "trace must include start and every traversed cell");
    ok &= Expect(trace.front() == SlideCell{0, 0}, "trace must begin at start cell");
    ok &= Expect(trace.back() == SlideCell{2, 0}, "trace must end at stop cell");

    const MiniGameCollection::Vec2 center = board.CellToWorld({2, 1});
    ok &= Expect(std::abs(center.x) < 0.0001f, "center column must map to world x zero");
    ok &= Expect(std::abs(center.y + 1.0f) < 0.0001f, "even-height grid mapping must remain centered");

    if (!ok) {
        return 1;
    }
    std::cout << "Backshot sliding movement contracts passed\n";
    return 0;
}
