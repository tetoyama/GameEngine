#include "Game/MiniGameCollection/Backshot/BackshotModel.h"
#include "Game/MiniGameCollection/ColorTerritory/ColorTerritoryModel.h"
#include "Game/MiniGameCollection/Core/MiniGameCore.h"
#include "Game/MiniGameCollection/SheepRoundup/SheepSteeringModel.h"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

class DummyRules final : public MiniGameCollection::IMiniGameRules {
public:
    void Prepare() override {
        prepared = true;
    }

    void StartGame() override {
        assert(prepared);
        started = true;
    }

    void Tick(float deltaTime) override {
        assert(started);
        elapsed += deltaTime;
    }

    bool IsFinished() const override {
        return elapsed >= 0.1f;
    }

    MiniGameCollection::MiniGameResult BuildResult() const override {
        MiniGameCollection::MiniGameResult result;
        result.gameId = MiniGameCollection::MiniGameId::ColorTerritory;
        result.players = {
            {.playerId = 0, .score = 5},
            {.playerId = 1, .score = 3}
        };
        return result;
    }

    void Shutdown() override {
        shutdown = true;
    }

    bool prepared = false;
    bool started = false;
    bool shutdown = false;
    float elapsed = 0.0f;
};

void TestSessionAndResult() {
    using namespace MiniGameCollection;

    DummyRules rules;
    MiniGameSession session;
    session.BeginLoading(MiniGameId::ColorTerritory, 42);
    session.AttachRules(rules);
    session.EnterIntroduction();
    session.BeginCountdown(1.0f);

    session.Tick(0.0f, 0.4f);
    assert(session.GetState() == MiniGameState::Countdown);
    assert(!session.IsInputEnabled());

    session.Tick(0.0f, 0.6f);
    assert(session.GetState() == MiniGameState::Playing);
    assert(session.IsInputEnabled());

    session.Tick(0.1f, 0.1f);
    assert(session.GetState() == MiniGameState::Finishing);
    assert(!session.IsInputEnabled());

    session.Tick(0.0f, 0.65f);
    assert(session.GetState() == MiniGameState::Result);
    assert(session.HasResult());
    assert(session.TryGetResult()->players.front().playerId == 0);
    assert(session.TryGetResult()->players.front().rank == 1);

    session.RequestRetry();
    assert(session.GetState() == MiniGameState::Transition);
    assert(session.GetTransitionRequest() == TransitionRequest::Retry);

    session.ShutdownRules();
    assert(rules.shutdown);

    MiniGameResult tie;
    tie.players = {
        {.playerId = 1, .score = 8, .finishTimeSeconds = 2.0f},
        {.playerId = 0, .score = 8, .finishTimeSeconds = 2.0f},
        {.playerId = 2, .score = 6, .finishTimeSeconds = 1.0f}
    };
    tie.RebuildRanking();
    assert(tie.isTie);
    assert(tie.players[0].rank == 1);
    assert(tie.players[1].rank == 1);
    assert(tie.players[2].rank == 3);
}

void TestPresentationTimeline() {
    using namespace MiniGameCollection;

    PresentationTimeline timeline;
    timeline.Reset(77);
    timeline.Schedule(PresentationEventType::Countdown3, 0.0f);
    timeline.Schedule(PresentationEventType::Countdown2, 1.0f);
    timeline.Schedule(PresentationEventType::Countdown1, 2.0f);
    timeline.Schedule(PresentationEventType::Go, 3.0f, 1.5f);

    auto fired = timeline.Tick(0.0f);
    assert(fired.size() == 1);
    assert(fired[0].type == PresentationEventType::Countdown3);

    fired = timeline.Tick(2.0f);
    assert(fired.size() == 2);
    assert(fired[0].type == PresentationEventType::Countdown2);
    assert(fired[1].type == PresentationEventType::Countdown1);

    timeline.CancelAllForScene(77);
    assert(timeline.PendingCount() == 0);
}

void TestColorTerritory() {
    using namespace MiniGameCollection;
    using namespace MiniGameCollection::ColorTerritory;

    TerritoryBoard board(5, 5, 3);
    assert(board.CountUnclaimed() == 25);

    auto paint = board.Paint({2, 2}, 0);
    assert(paint.changed);
    assert(board.GetScore(0) == 1);

    paint = board.Paint({2, 2}, 1);
    assert(paint.changed);
    assert(paint.previousOwner == 0);
    assert(board.GetScore(0) == 0);
    assert(board.GetScore(1) == 1);

    board.Paint({2, 1}, 0);
    board.Paint({1, 2}, 0);
    board.Paint({3, 2}, 0);
    board.Paint({2, 3}, 0);
    assert(board.FindLeader() == 0);

    CpuTargetContext context;
    context.self = 1;
    context.currentTile = {2, 2};
    context.remainingTimeRatio = 0.05f;
    context.crowdByTile.resize(board.GetTileCount(), 0);

    const auto decision = TerritoryCpuEvaluator::ChooseTarget(
        board,
        context,
        CpuDifficultyProfile::Hard()
    );
    assert(decision.has_value());
    assert(decision->attacksLeader);
}

void TestSheepSteering() {
    using namespace MiniGameCollection;
    using namespace MiniGameCollection::SheepRoundup;

    SheepSteeringInput input;
    input.position = {-9.7f, 0.0f};
    input.previousDirection = {-1.0f, 0.0f};
    input.playerPositions = {{-8.8f, 0.0f}};
    input.flockPositions = {{-8.0f, 0.5f}, {-8.2f, -0.4f}};
    input.movementBounds = {{-10.0f, -10.0f}, {10.0f, 10.0f}};

    SheepSteeringConfig config;
    config.wallAvoidanceWeight = 3.5f;
    const SheepSteeringOutput output = SheepSteeringModel::Compute(
        input,
        config,
        0.25f
    );

    assert(output.avoidingWall);
    assert(output.velocity.x > 0.0f);
    assert(std::abs(Length(output.direction) - 1.0f) < 0.001f);

    const std::vector<SheepTargetCandidate> sheep = {
        {.sheepIndex = 0, .sheepPosition = {4.0f, 0.0f}},
        {.sheepIndex = 1, .sheepPosition = {1.0f, 0.0f}}
    };
    SheepCpuContext cpu;
    cpu.cpuPosition = {0.0f, 0.0f};
    cpu.ownPenCenter = {-8.0f, 0.0f};
    const auto target = SheepCpuEvaluator::ChooseSheep(
        sheep,
        cpu,
        CpuDifficultyProfile::Normal()
    );
    assert(target.has_value());
    assert(target->sheepIndex == 1);
    assert(target->interceptPosition.x > 1.0f);
}

void TestBackshot() {
    using namespace MiniGameCollection;
    using namespace MiniGameCollection::Backshot;

    BackshotConfig config;
    CombatantSnapshot attacker{
        .playerId = 0,
        .position = {0.0f, -2.0f},
        .forward = {0.0f, 1.0f}
    };
    CombatantSnapshot victim{
        .playerId = 1,
        .position = {0.0f, 0.0f},
        .forward = {0.0f, 1.0f}
    };

    ShotResult result = BackshotHitResolver::Resolve(
        attacker,
        victim,
        true,
        config
    );
    assert(result.resolution == ShotResolution::RearElimination);
    assert(result.victimRearDot < -0.99f);

    attacker.position = {0.0f, 2.0f};
    attacker.forward = {0.0f, -1.0f};
    result = BackshotHitResolver::Resolve(attacker, victim, true, config);
    assert(result.resolution == ShotResolution::FrontOrSideGuard);
    assert(result.victimRearDot > 0.99f);

    BackshotCpuContext cpu;
    cpu.self = {
        .playerId = 0,
        .position = {0.0f, -2.0f},
        .forward = {0.0f, 1.0f}
    };
    cpu.candidates = {
        {
            .combatant = victim,
            .hasLineOfSight = true,
            .isTargetingSelf = false
        }
    };

    const auto decision = BackshotCpuEvaluator::Evaluate(
        cpu,
        config,
        CpuDifficultyProfile::Normal()
    );
    assert(decision.has_value());
    assert(decision->target == 1);
    assert(decision->shouldShoot);
}

} // namespace

int main() {
    TestSessionAndResult();
    TestPresentationTimeline();
    TestColorTerritory();
    TestSheepSteering();
    TestBackshot();
    return 0;
}
