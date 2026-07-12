#include "Game/MiniGameCollection/Backshot/BackshotRules.h"
#include "Game/MiniGameCollection/ColorTerritory/ColorTerritoryRules.h"
#include "Game/MiniGameCollection/Core/MiniGameCollectionManagerModel.h"
#include "Game/MiniGameCollection/Core/MiniGameCpuDecisionClock.h"
#include "Game/MiniGameCollection/Core/MiniGamePlayerModel.h"
#include "Game/MiniGameCollection/Core/MiniGameSceneTransition.h"
#include "Game/MiniGameCollection/SheepRoundup/SheepRoundupRules.h"

#include <cassert>
#include <cmath>
#include <string>

namespace {

class FakeSceneTransitionBackend final
    : public MiniGameCollection::IMiniGameSceneTransitionBackend {
public:
    void SetGameplayInputEnabled(bool enabled) override {
        inputEnabled = enabled;
    }

    void CancelPresentation(MiniGameCollection::SceneToken sceneToken) override {
        assert(sceneToken == 55);
        presentationCancelled = true;
    }

    void ShutdownRules(MiniGameCollection::SceneToken sceneToken) override {
        assert(sceneToken == 55);
        rulesShutdown = true;
    }

    bool RequestUnloadScene(const std::string& sceneName) override {
        assert(sceneName == "ColorTerritory");
        unloadRequested = true;
        return true;
    }

    bool IsSceneUnloaded(const std::string& sceneName) const override {
        assert(sceneName == "ColorTerritory");
        return unloadComplete;
    }

    bool RequestLoadScene(const std::string& scenePath) override {
        assert(scenePath == "Asset/Game/Selection.scene");
        loadRequested = true;
        return true;
    }

    bool IsSceneLoaded(const std::string& scenePath) const override {
        assert(scenePath == "Asset/Game/Selection.scene");
        return loadComplete;
    }

    bool inputEnabled = true;
    bool presentationCancelled = false;
    bool rulesShutdown = false;
    bool unloadRequested = false;
    bool unloadComplete = false;
    bool loadRequested = false;
    bool loadComplete = false;
};

void TestSharedPlayerMovement() {
    using namespace MiniGameCollection;

    MiniGamePlayerState player;
    player.playerId = 0;
    player.inputEnabled = true;

    MiniGamePlayerConfig config;
    MovementBounds bounds{{-2.0f, -2.0f}, {2.0f, 2.0f}};
    MiniGamePlayerModel::Tick(
        player,
        {.move = {1.0f, 0.0f}},
        config,
        bounds,
        0.25f
    );
    assert(player.velocity.x > 0.0f);
    assert(player.position.x > 0.0f);
    assert(player.forward.x > 0.9f);

    MiniGamePlayerState other;
    other.playerId = 1;
    other.position = player.position;
    other.inputEnabled = true;
    MiniGamePlayerModel::ResolveSoftContact(player, other, config);
    assert(Distance(player.position, other.position) > 0.0f);
    assert(Length(player.knockbackVelocity) > 0.0f);
}

void TestCpuDecisionClock() {
    using namespace MiniGameCollection;

    MiniGameCpuDecisionClock lhs(CpuDifficultyProfile::Normal(), 1234);
    MiniGameCpuDecisionClock rhs(CpuDifficultyProfile::Normal(), 1234);
    lhs.Reset();
    rhs.Reset();

    assert(lhs.Tick(0.0f));
    assert(rhs.Tick(0.0f));
    assert(std::abs(
        lhs.GetDecisionRemainingSeconds() -
        rhs.GetDecisionRemainingSeconds()
    ) < 0.00001f);

    lhs.CommitTarget();
    assert(!lhs.CanChangeTarget());
    lhs.Tick(2.0f);
    assert(lhs.CanChangeTarget());
}

void TestSceneTransition() {
    using namespace MiniGameCollection;

    FakeSceneTransitionBackend backend;
    MiniGameSceneTransition transition(backend);
    transition.Begin({
        .sourceSceneToken = 55,
        .sourceSceneName = "ColorTerritory",
        .targetScenePath = "Asset/Game/Selection.scene",
        .reason = TransitionRequest::Selection,
        .presentationWaitSeconds = 0.2f
    });

    transition.Tick(0.0f);
    assert(!backend.inputEnabled);
    transition.Tick(0.2f);
    transition.Tick(0.0f);
    assert(backend.presentationCancelled);
    transition.Tick(0.0f);
    assert(backend.rulesShutdown);
    transition.Tick(0.0f);
    assert(backend.unloadRequested);

    backend.unloadComplete = true;
    transition.Tick(0.0f);
    transition.Tick(0.0f);
    assert(backend.loadRequested);

    backend.loadComplete = true;
    transition.Tick(0.0f);
    assert(transition.IsComplete());
}

void TestCollectionSelection() {
    using namespace MiniGameCollection;

    MiniGameCollectionManagerModel manager;
    assert(manager.GetGameCount() == 3);
    assert(manager.SelectGame(MiniGameId::SheepRoundup));
    assert(manager.GetSelectedGame().gameId == MiniGameId::SheepRoundup);
    assert(manager.BeginSelectedGame(90));
    assert(!manager.SelectGame(MiniGameId::Backshot));
    assert(manager.FinishSession(TransitionRequest::NextGame));
    assert(manager.GetSelectedGame().gameId == MiniGameId::Backshot);
}

void TestColorTerritoryRules() {
    using namespace MiniGameCollection;
    using namespace MiniGameCollection::ColorTerritory;

    ColorTerritoryRules rules(3, 3, 2, 1.0f);
    rules.Prepare();
    rules.StartGame();

    assert(rules.SubmitPlayerTile(0, {1, 1}));
    rules.Tick(0.1f);
    assert(rules.TryGetBoard()->GetScore(0) == 1);

    assert(rules.SubmitPlayerTile(1, {1, 1}));
    rules.Tick(0.1f);
    assert(rules.TryGetBoard()->GetScore(0) == 0);
    assert(rules.TryGetBoard()->GetScore(1) == 1);

    rules.Tick(0.8f);
    assert(rules.IsFinished());
    const MiniGameResult result = rules.BuildResult();
    assert(result.players.front().playerId == 1);
    assert(result.players.front().score == 1);
    rules.Shutdown();
}

void TestSheepRoundupRules() {
    using namespace MiniGameCollection;
    using namespace MiniGameCollection::SheepRoundup;

    SheepRoundupRules rules(2, 5.0f, {{-5.0f, -5.0f}, {5.0f, 5.0f}});
    rules.SetPens({
        {.owner = 0, .center = {-4.0f, 0.0f}, .radius = 1.0f},
        {.owner = 1, .center = {4.0f, 0.0f}, .radius = 1.0f}
    });
    rules.SetInitialSheep({{-4.0f, 0.0f}});
    rules.Prepare();
    rules.StartGame();
    rules.SetPlayerPosition(0, {0.0f, 4.0f});
    rules.SetPlayerPosition(1, {0.0f, -4.0f});
    rules.Tick(0.05f);

    assert(rules.IsFinished());
    assert(rules.GetScores()[0] == 1);
    const auto events = rules.ConsumeScoreEvents();
    assert(events.size() == 1);
    assert(events[0].playerId == 0);
    rules.Shutdown();
}

void TestBackshotRules() {
    using namespace MiniGameCollection;
    using namespace MiniGameCollection::Backshot;

    BackshotRules rules(2, 10.0f);
    rules.Prepare();
    rules.UpdateCombatant(0, {0.0f, -2.0f}, {0.0f, 1.0f});
    rules.UpdateCombatant(1, {0.0f, 0.0f}, {0.0f, 1.0f});
    rules.StartGame();

    assert(rules.QueueShot(0, 1, true));
    rules.Tick(0.01f);
    assert(rules.IsFinished());
    assert(rules.GetLivingPlayerCount() == 1);
    assert(rules.GetEliminations()[0] == 1);

    const MiniGameResult result = rules.BuildResult();
    assert(result.players.front().playerId == 0);
    assert(!result.players.front().eliminated);
    assert(result.players.front().score == 1);
    rules.Shutdown();
}

} // namespace

int main() {
    TestSharedPlayerMovement();
    TestCpuDecisionClock();
    TestSceneTransition();
    TestCollectionSelection();
    TestColorTerritoryRules();
    TestSheepRoundupRules();
    TestBackshotRules();
    return 0;
}
