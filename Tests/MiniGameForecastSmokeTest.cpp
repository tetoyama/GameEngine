#include "Game/MiniGameCollection/ColorTerritory/ColorTerritoryItemModel.h"
#include "Game/MiniGameCollection/SheepRoundup/SheepRoundupRules.h"

#include <cassert>
#include <cmath>
#include <optional>
#include <vector>

namespace {

void TestColorForecastMatchesDeterministicChoice() {
    using namespace MiniGameCollection::ColorTerritory;

    std::optional<TerritoryItemForecast> observed;
    TerritoryItemRandom::SetForecastObserver(
        [&observed](const TerritoryItemForecast& forecast) {
            observed = forecast;
        }
    );

    TerritoryItemRandom random(0x12345678u);
    const TileCoord tile = random.NextTile(11, 7);
    const TerritoryItemType type = random.NextType();

    assert(observed.has_value());
    assert(observed->tile == tile);
    assert(observed->type == type);

    TerritoryItemConfig config;
    assert(config.fallingSeconds >= 2.8f);
    assert(config.fallingSeconds >= 2.4f);
    TerritoryItemRandom::ClearForecastObserver();
}

void TestSheepWarningReservationMatchesActualSpawn() {
    using namespace MiniGameCollection;
    using namespace MiniGameCollection::SheepRoundup;

    std::optional<SheepSpawnWarning> observed;
    SheepRoundupRules::SetSpawnWarningObserver(
        [&observed](const SheepSpawnWarning& warning) {
            observed = warning;
        }
    );

    SheepRoundupRules rules(
        2,
        20.0f,
        {{-6.0f, -4.0f}, {6.0f, 4.0f}}
    );
    rules.SetPens({
        {.owner = 0, .center = {-5.0f, 0.0f}, .radius = 0.6f},
        {.owner = 1, .center = {5.0f, 0.0f}, .radius = 0.6f}
    });
    rules.SetInitialSheepDefinitions({
        {.position = {0.0f, 0.0f}, .golden = false}
    });

    SheepSpawnConfig config;
    config.endlessSpawning = true;
    config.poolCapacity = 2;
    config.earlyTargetActive = 2;
    config.lateTargetActive = 2;
    config.earlySpawnIntervalSeconds = 0.01f;
    config.lateSpawnIntervalSeconds = 0.01f;
    config.earlySpawnBatch = 1;
    config.lateSpawnBatch = 1;
    config.earlyGoldenChance = 1.0f;
    config.lateGoldenChance = 1.0f;
    config.normalSpawnWarningSeconds = 0.65f;
    config.goldenSpawnWarningSeconds = 2.2f;
    rules.SetSpawnConfig(config);
    rules.SetSpawnSeed(0xABCD1234u);

    rules.Prepare();
    rules.StartGame();
    rules.SetPlayerPosition(0, {-5.0f, -3.0f});
    rules.SetPlayerPosition(1, {5.0f, 3.0f});
    rules.Tick(0.02f);

    assert(observed.has_value());
    assert(observed->golden);
    assert(std::abs(observed->warningSeconds - 2.2f) < 0.0001f);
    assert(rules.GetPendingSpawnCount() == 1);
    assert(rules.GetActiveSheepCount() == 1);

    rules.Tick(2.1f);
    assert(rules.GetPendingSpawnCount() == 1);
    assert(rules.GetActiveSheepCount() == 1);
    assert(rules.ConsumeSpawnEvents().empty());

    rules.Tick(0.1f);
    assert(rules.GetPendingSpawnCount() == 0);
    assert(rules.GetActiveSheepCount() == 2);

    const std::vector<SheepSpawnEvent> events = rules.ConsumeSpawnEvents();
    assert(events.size() == 1);
    assert(events.front().sheepId == observed->sheepId);
    assert(events.front().generation == observed->nextGeneration);
    assert(events.front().golden == observed->golden);
    assert(Distance(events.front().position, observed->position) < 0.0001f);

    rules.Shutdown();
    SheepRoundupRules::ClearSpawnWarningObserver();
}

} // namespace

int main() {
    TestColorForecastMatchesDeterministicChoice();
    TestSheepWarningReservationMatchesActualSpawn();
    return 0;
}
